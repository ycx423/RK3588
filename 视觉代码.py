import sensor, image, time, pyb
from pyb import UART
import json

# === 1. 硬件初始化 ===
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)  # 320x240分辨率
sensor.set_windowing((160, 120, 160, 120))  # 中心区域ROI
sensor.skip_frames(time=2000)      # 等待摄像头稳定
sensor.set_auto_gain(True)         # 启用自动增益
sensor.set_auto_whitebal(True)     # 启用自动白平衡
sensor.set_auto_exposure(True)     # 启用自动曝光

# LED补光配置
red_led = pyb.LED(1)
green_led = pyb.LED(2)
blue_led = pyb.LED(3)
# 开启白色补光 (红+绿+蓝)
red_led.on()
green_led.on()
blue_led.on()

uart = UART(3, 115200)  # 串口配置
clock = time.clock()     # 帧率监测

# === 2. pH全范围颜色阈值配置 ===
# 优化阈值范围，提高检测灵敏度
PH_CONFIGS = {
    # 强酸性(pH1-3)：红色/橙色
    "pH1": {"threshold": (25, 100, 25, 127, 10, 90), "color": (255, 0, 0),   "name": "红"},
    "pH2": {"threshold": (30, 95, 20, 120, 15, 100), "color": (255, 50, 0),  "name": "深红"},
    "pH3": {"threshold": (35, 90, 10, 110, 25, 110), "color": (255, 100, 0), "name": "橙红"},

    # 弱酸性(pH4-6)：黄色/黄绿色
    "pH4": {"threshold": (40, 85, 0, 100, 40, 120),  "color": (255, 150, 0), "name": "橙黄"},
    "pH5": {"threshold": (45, 80, -10, 90, 50, 130), "color": (255, 200, 0), "name": "黄"},
    "pH6": {"threshold": (40, 75, -15, 40, 35, 110), "color": (150, 255, 0), "name": "黄绿"},

    # 中性(pH7)：绿色
    "pH7": {"threshold": (35, 70, -25, 25, 25, 90), "color": (0, 255, 0),   "name": "绿"},

    # 弱碱性(pH8-10)：青色/蓝色
    "pH8": {"threshold": (30, 65, -35, 15, 0, 80),   "color": (0, 200, 200), "name": "青"},
    "pH9": {"threshold": (25, 60, -45, 5, -15, 70),  "color": (0, 100, 255), "name": "蓝青"},
    "pH10": {"threshold": (20, 55, -55, -5, -25, 60), "color": (0, 0, 200),  "name": "蓝"},

    # 强碱性(pH11-14)：紫色/靛色
    "pH11": {"threshold": (15, 50, -65, -15, -35, 50), "color": (100, 0, 150), "name": "紫蓝"},
    "pH12": {"threshold": (10, 45, -75, -25, -45, 40), "color": (150, 0, 150), "name": "紫"},
    "pH13": {"threshold": (5, 40, -85, -35, -55, 30), "color": (200, 0, 150), "name": "深紫"},
    "pH14": {"threshold": (0, 35, -95, -45, -65, 20), "color": (255, 0, 150), "name": "紫红"}
}

# === 3. 高效检测参数配置 ===
MIN_BLOB_AREA = 50        # 降低最小面积阈值以提高灵敏度
PIXEL_THRESHOLD = 15      # 降低最小像素数
MERGE_MARGIN = 1          # 减小色块合并边缘
ROI = (20, 10, 120, 100)  # 扩大ROI区域

# === 4. 核心检测函数优化 ===
def detect_ph_value(img):
    """
    高效检测图像中的pH色块
    返回: (最佳匹配的pH标签, 色块对象, 配置信息)
    """
    # 使用更灵敏的阈值检测色块
    max_blob = None
    best_config = None
    best_ph = None
    max_area = 0

    # 遍历所有pH配置
    for ph_label, config in PH_CONFIGS.items():
        # 使用更灵敏的参数
        blobs = img.find_blobs(
            [config["threshold"]],
            roi=ROI,
            x_stride=1,            # 减小步长以提高灵敏度
            y_stride=1,
            pixels_threshold=PIXEL_THRESHOLD,
            area_threshold=MIN_BLOB_AREA,
            merge=True,
            margin=MERGE_MARGIN,
            invert=False
        )

        if blobs:
            # 取当前配置下最大的色块
            largest_blob = max(blobs, key=lambda b: b.area())

            # 选择面积最大的色块
            if largest_blob.area() > max_area:
                max_area = largest_blob.area()
                max_blob = largest_blob
                best_config = config
                best_ph = ph_label

    return (best_ph, max_blob, best_config)

# === 5. 主循环优化 ===
last_ph = None
last_blob = None
last_config = None
stable_count = 0
STABLE_THRESHOLD = 3  # 连续3次相同结果视为稳定
missed_frames = 0
MAX_MISSED_FRAMES = 10  # 最多允许连续10帧未检测到色块
last_stable_ph = None  # 记录最后一次稳定的pH值

# 硬件复位函数
def reset_hardware():
    print("执行硬件复位...")
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)
    sensor.set_windowing((160, 120, 160, 120))
    sensor.skip_frames(time=1000)
    sensor.set_auto_gain(True)
    sensor.set_auto_whitebal(True)
    sensor.set_auto_exposure(True)
    red_led.on()
    green_led.on()
    blue_led.on()
    clock.tick()  # 重置帧率计时

while True:
    clock.tick()
    img = sensor.snapshot()

    # 执行pH检测
    ph_label, blob, config = detect_ph_value(img)

    # 如果检测到色块
    if blob and config and ph_label:
        # 重置未检测帧计数器
        missed_frames = 0

        # 检查是否与上次结果相同
        if ph_label == last_ph:
            stable_count = min(stable_count + 1, STABLE_THRESHOLD)
        else:
            stable_count = 0

        last_ph = ph_label
        last_blob = blob
        last_config = config

        # 如果达到稳定状态，记录稳定值
        if stable_count >= STABLE_THRESHOLD:
            last_stable_ph = ph_label
    else:
        missed_frames += 1
        # 如果连续多帧未检测到，重置稳定计数器
        if missed_frames > MAX_MISSED_FRAMES:
            stable_count = 0
            last_ph = None

    # 处理检测结果 - 只在有有效色块时绘制
    if blob and config and ph_label:
        # 绘制检测结果
        img.draw_rectangle(blob.rect(), color=config["color"], thickness=2)
        img.draw_cross(blob.cx(), blob.cy(), color=config["color"], size=6)

        # 显示pH值
        info_text = f"{ph_label} ({config['name']})"
        img.draw_string(
            max(0, blob.x() - 5),
            max(0, blob.y() - 15),
            info_text,
            color=config["color"],
            scale=1.0,
            mono_space=False
        )

    # 在画面底部显示大号pH值（如果稳定）
    if stable_count >= STABLE_THRESHOLD and last_stable_ph:
        ph_label = last_stable_ph
        config = PH_CONFIGS.get(ph_label, {"color": (255, 255, 255)})
        img.draw_string(img.width()//2 - 25, img.height() - 25,
                       f"pH: {ph_label}",
                       color=config["color"], scale=2.0)

        # 准备JSON数据
        if blob:  # 确保blob存在
            result = {
                "pH": ph_label,
                "color": config["name"],
                "position": (blob.cx(), blob.cy()),
                "area": blob.area()
            }
        else:
            result = {
                "pH": ph_label,
                "color": config["name"],
                "status": "stable_no_blob"
            }

        # 通过串口发送结果
        try:
            uart.write(json.dumps(result) + "\n")
            if blob:
                print(f"检测到: {ph_label} ({config['name']}) | 位置: {blob.cx()},{blob.cy()} | 面积: {blob.area()}")
            else:
                print(f"稳定状态: {ph_label} ({config['name']})")
        except Exception as e:
            print("串口发送失败:", e)
    elif blob and config and ph_label:
        print(f"检测中: {ph_label} ({config['name']}) | 稳定度: {stable_count}/{STABLE_THRESHOLD}")
    else:
        print(f"未检测到pH试纸 | 帧率: {clock.fps():.1f}FPS")

    # 显示帧率
    fps_text = f"FPS: {clock.fps():.1f}"
    img.draw_string(5, 5, fps_text, color=(255, 255, 255), scale=1.0)

    # 显示检测状态
    if stable_count >= STABLE_THRESHOLD:
        status_text = "稳定"
        status_color = (0, 255, 0)  # 绿色
    elif blob:
        status_text = "检测中"
        status_color = (255, 255, 0)  # 黄色
    else:
        status_text = "未检测"
        status_color = (255, 0, 0)  # 红色

    img.draw_string(img.width() - 70, 5, status_text, color=status_color, scale=1.0)

    # 绘制ROI区域
    img.draw_rectangle(ROI[0], ROI[1], ROI[2], ROI[3], color=(100, 100, 100))

    # 添加调试信息
    if blob:
        # 显示色块面积
        area_text = f"Area: {blob.area()}"
        img.draw_string(5, img.height() - 20, area_text, color=(255, 255, 255), scale=0.8)

        # 显示色块中心LAB值
        center_color = img.get_pixel(blob.cx(), blob.cy())
        l, a, b = image.rgb_to_lab(center_color)
        lab_text = f"L:{l:.0f} A:{a:.0f} B:{b:.0f}"
        img.draw_string(5, img.height() - 35, lab_text, color=(255, 255, 255), scale=0.8)

    # 定期检查帧率，过低时复位
    if clock.fps() < 5 and clock.fps() > 0:  # 避免除零错误
        print(f"帧率过低 ({clock.fps():.1f}FPS)，执行复位...")
        reset_hardware()
