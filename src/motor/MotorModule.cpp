#include "motor/MotorModule.h"

#include <Arduino.h>

#include "LogSwitch.h"
#include "ultrasonic/ultrasonic.h"

namespace esp32s3
{
#define MOTOR_PIN_1 39
#define MOTOR_PIN_2 40
#define MOTOR_PIN_3 41
#define MOTOR_PIN_4 42
#define MOTOR_PIN_5 45

// 5个马达在腰带上的固定角度（0°为正前方，顺时针为正）
static const int MOTOR_ANGLES[5] = {36, 108, 180, 252, 324};
static const int MOTOR_PINS[5] = {MOTOR_PIN_1, MOTOR_PIN_2, MOTOR_PIN_3,
                                  MOTOR_PIN_4, MOTOR_PIN_5};

typedef enum
{
  MOTOR_CMD_STOP,
  MOTOR_CMD_START,
} MotorCmdType;

typedef struct
{
  MotorCmdType cmd;
  int angle;
} MotorCommand;

QueueHandle_t motorCmdQueue = NULL;
bool navRunning = false;    // 导航震动是否激活
bool obsRunning = false;    // 避障震动是否激活
int navTargetAngle = 0;
bool s_motorInitialized = false;

/**
 * @brief 找到离目标角度最近的那个马达索引
 * @param angle 目标角度 (0~360)
 * @return 马达索引 0~4
 */
int findNearestMotor(int angle)
{
  // 规范化到 [0, 360)
  angle = (angle % 360 + 360) % 360;

  int bestIdx = 0;
  int bestDist = 360;

  for (int i = 0; i < 5; i++)
  {
    int diff = abs(angle - MOTOR_ANGLES[i]);
    if (diff > 180)
    {
      diff = 360 - diff;
    }
    if (diff < bestDist)
    {
      bestDist = diff;
      bestIdx = i;
    }
  }
  return bestIdx;
}

/**
 * @brief 设置单个马达的开关状态
 */
void setMotor(int index, bool on)
{
  if (index < 0 || index >= 5) return;
  digitalWrite(MOTOR_PINS[index], on ? HIGH : LOW);
}

/**
 * @brief 根据布尔数组设置5个马达状态
 */
void setMotors(bool m[5])
{
  for (int i = 0; i < 5; i++)
  {
    digitalWrite(MOTOR_PINS[i], m[i] ? HIGH : LOW);
  }
}

void stopAllMotors()
{
  bool off[5] = {false, false, false, false, false};
  setMotors(off);
  LOG_PRINTLN(LOG_MOTOR, "[Motor] 所有马达已停止");
}

void initMotorPins()
{
  for (int i = 0; i < 5; i++)
  {
    pinMode(MOTOR_PINS[i], OUTPUT);
  }
  stopAllMotors();
  LOG_PRINTLN(LOG_MOTOR, "[Motor] 5个马达引脚初始化完成");
}

void motorTask(void* pvParameters)
{
  MotorCommand motorCmd;

  // 导航震动独立状态
  bool navState = false;          // 当前1s周期内的开关状态
  unsigned long navLastToggle = 0;
  int navMotorIdx = -1;           // 当前导航激活的马达索引

  // 避障震动独立状态（两个超声波各自独立消抖）
  unsigned long obsLastTrigM1Time = 0;  // 超声波0最后一次触发时间
  unsigned long obsLastTrigM5Time = 0;  // 超声波1最后一次触发时间

  LOG_PRINTLN(LOG_MOTOR, "[Motor Task] 任务已启动, 运行在 Core 0");

  while (true)
  {
    // ── 处理命令队列 ──
    if (xQueueReceive(motorCmdQueue, &motorCmd, 0) == pdPASS)
    {
      if (motorCmd.cmd == MOTOR_CMD_START)
      {
        navRunning = true;
        navTargetAngle = motorCmd.angle;
        navMotorIdx = findNearestMotor(navTargetAngle);
        navState = true;
        navLastToggle = millis();

        LOG_PRINTF(LOG_MOTOR,
                   "[Motor] 导航开始 | 角度: %d | 最近马达: M%d (位置%d°)\n",
                   navTargetAngle, navMotorIdx + 1,
                   MOTOR_ANGLES[navMotorIdx]);
      }
      else if (motorCmd.cmd == MOTOR_CMD_STOP)
      {
        navRunning = false;
        navMotorIdx = -1;
        LOG_PRINTLN(LOG_MOTOR, "[Motor] 导航震动停止");
      }
    }

    // ── 导航震动：震1秒停1秒 ──
    if (navRunning)
    {
      unsigned long now = millis();
      if (now - navLastToggle >= 1000)
      {
        navState = !navState;
        navLastToggle = now;

        if (navState)
        {
          navMotorIdx = findNearestMotor(navTargetAngle);
          LOG_PRINTF(LOG_MOTOR, "[Motor] 导航震动 ON -> M%d\n",
                     navMotorIdx + 1);
        }
        else
        {
          LOG_PRINTLN(LOG_MOTOR, "[Motor] 导航震动 OFF");
        }
      }
    }

    // ── 避障检测：两个超声波各自独立消抖，互不干扰 ──
    {
      unsigned long now = millis();
      float dist0 = UltrasonicModule::getLatestDistanceMm0();
      float dist1 = UltrasonicModule::getLatestDistanceMm1();
      bool trigObsM1 = (dist0 > 0.0f && dist0 < 1000.0f);
      bool trigObsM5 = (dist1 > 0.0f && dist1 < 1000.0f);

      // 超声波0 → M1：有障碍立即刷新，无障碍等1秒消抖再关闭
      if (trigObsM1)
      {
        obsLastTrigM1Time = now;
        LOG_PRINTF(LOG_ULTRASONIC,
                   "[Ultrasonic] 超声波0 %.1fmm < 1m → M1 避障震动\n",
                   (double)dist0);
      }
      bool obsM1Active = (now - obsLastTrigM1Time < 1000);

      // 超声波1 → M5：有障碍立即刷新，无障碍等1秒消抖再关闭
      if (trigObsM5)
      {
        obsLastTrigM5Time = now;
        LOG_PRINTF(LOG_ULTRASONIC,
                   "[Ultrasonic] 超声波1 %.1fmm < 1m → M5 避障震动\n",
                   (double)dist1);
      }
      bool obsM5Active = (now - obsLastTrigM5Time < 1000);

      obsRunning = (obsM1Active || obsM5Active);
    }

    // ── 综合输出：导航和避障取"或"，统一设置马达 ──
    {
      bool m[5];
      for (int i = 0; i < 5; i++) m[i] = false;

      // 导航需求
      if (navRunning && navState && navMotorIdx >= 0)
      {
        m[navMotorIdx] = true;
      }

      // 避障需求（独立消抖：最近1秒内有障碍物就震动）
      unsigned long now = millis();
      if (now - obsLastTrigM1Time < 1000) m[0] = true;
      if (now - obsLastTrigM5Time < 1000) m[4] = true;

      setMotors(m);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

bool enqueueMotorCmd(MotorCmdType cmd, int angle)
{
  if (motorCmdQueue == NULL)
  {
    return false;
  }

  MotorCommand motorCmd;
  motorCmd.cmd = cmd;
  motorCmd.angle = angle;
  return xQueueSend(motorCmdQueue, &motorCmd, 0) == pdPASS;
}

static bool initImpl()
{
  randomSeed((uint32_t)esp_random());

  LOG_PRINTLN(LOG_MOTOR, "[Setup] 初始化振动马达引脚...");
  initMotorPins();

  LOG_PRINTLN(LOG_MOTOR, "[Setup] 创建Motor队列...");
  motorCmdQueue = xQueueCreate(5, sizeof(MotorCommand));
  if (motorCmdQueue == NULL)
  {
    LOG_PRINTLN(LOG_MOTOR, "[Setup] 错误: Motor队列创建失败!");
    return false;
  }

  s_motorInitialized = true;
  return true;
}

static bool startTasksImpl()
{
  if (!s_motorInitialized)
  {
    LOG_PRINTLN(LOG_MOTOR, "[Setup] 错误: motor 模块尚未初始化");
    return false;
  }

  BaseType_t ok =
      xTaskCreatePinnedToCore(motorTask, "Motor_Task", 4096, NULL, 1, NULL, 0);
  if (ok != pdPASS)
  {
    LOG_PRINTLN(LOG_MOTOR, "[Setup] 错误: Motor任务创建失败");
    return false;
  }

  LOG_PRINTLN(LOG_MOTOR, "[Setup] Motor模块启动完成!");
  return true;
}
}  // namespace esp32s3

bool esp32s3::MotorModule::init() { return esp32s3::initImpl(); }
bool esp32s3::MotorModule::startTasks() { return esp32s3::startTasksImpl(); }

bool esp32s3::MotorModule::startByAngle(int angle)
{
  return esp32s3::enqueueMotorCmd(esp32s3::MOTOR_CMD_START, angle);
}

bool esp32s3::MotorModule::stop()
{
  return esp32s3::enqueueMotorCmd(esp32s3::MOTOR_CMD_STOP, 0);
}
