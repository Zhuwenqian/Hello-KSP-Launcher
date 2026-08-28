// 游戏设置键值表：settings.cfg 键名 -> 中/英文显示名/分类（含隐藏标记）
#include "instancemanager_keymap.h"
#include "configmanager.h"
#include <QMap>

namespace {

struct KeyInfo {
    QString displayName;   // 中文显示名
    QString displayNameEn; // 英文显示名
    QString category;
    bool hidden = false;
};

QMap<QString, KeyInfo> createKeyInfoMap() {
    QMap<QString, KeyInfo> m;
    // ===== 显示与图形 (Display & Graphics) =====
    m["SCREEN_RESOLUTION_WIDTH"] = {"屏幕宽度", "Screen Resolution Width", "显示与图形"};
    m["SCREEN_RESOLUTION_HEIGHT"] = {"屏幕高度", "Screen Resolution Height", "显示与图形"};
    m["FULLSCREEN"] = {"全屏模式", "Fullscreen", "显示与图形"};
    m["QUALITY_PRESET"] = {"画质预设", "Quality Preset", "显示与图形"};
    m["ANTI_ALIASING"] = {"抗锯齿", "Anti-Aliasing", "显示与图形"};
    m["TEXTURE_QUALITY"] = {"纹理质量", "Texture Quality", "显示与图形"};
    m["SYNC_VBL"] = {"垂直同步(VSync)", "Vertical Sync (VSync)", "显示与图形"};
    m["LIGHT_QUALITY"] = {"光照质量", "Light Quality", "显示与图形"};
    m["SHADOWS_QUALITY"] = {"阴影质量", "Shadow Quality", "显示与图形"};
    m["FRAMERATE_LIMIT"] = {"帧率限制", "Framerate Limit", "显示与图形"};
    m["SHADOWS_FLIGHT_PROJECTION"] = {"飞行阴影投影", "Flight Shadow Projection", "显示与图形"};
    m["SHADOWS_KSC_PROJECTION"] = {"KSC阴影投影", "KSC Shadow Projection", "显示与图形"};
    m["SHADOWS_TRACKING_PROJECTION"] = {"追踪站阴影投影", "Tracking Station Shadow Projection", "显示与图形"};
    m["SHADOWS_EDITORS_PROJECTION"] = {"编辑器阴影投影", "Editor Shadow Projection", "显示与图形"};
    m["SHADOWS_MAIN_PROJECTION"] = {"主菜单阴影投影", "Main Menu Shadow Projection", "显示与图形"};
    m["SHADOWS_DEFAULT_PROJECTION"] = {"默认阴影投影", "Default Shadow Projection", "显示与图形"};
    m["AMBIENTLIGHT_BOOSTFACTOR"] = {"环境光增强系数", "Ambient Light Boost Factor", "显示与图形"};
    m["AMBIENTLIGHT_BOOSTFACTOR_MAPONLY"] = {"地图环境光增强", "Ambient Light Boost (Map)", "显示与图形"};
    m["AMBIENTLIGHT_BOOSTFACTOR_EDITONLY"] = {"编辑器环境光增强", "Ambient Light Boost (Editor)", "显示与图形"};
    m["PLANET_SCATTER"] = {"行星表面散布", "Planet Scatter", "显示与图形"};
    m["PLANET_SCATTER_FACTOR"] = {"散布密度系数", "Scatter Density Factor", "显示与图形"};
    m["WATERLEVEL_BASE_OFFSET"] = {"水位基础偏移", "Water Level Base Offset", "显示与图形"};
    m["WATERLEVEL_MAXLEVEL_MULT"] = {"水位最大倍率", "Water Level Max Multiplier", "显示与图形"};
    m["UNSUPPORTED_LEGACY_SHADER_TERRAIN"] = {"使用旧版地形着色器", "Use Legacy Terrain Shader", "显示与图形"};
    m["AERO_FX_QUALITY"] = {"气动效果质量", "Aerodynamic Effects Quality", "显示与图形"};
    m["SURFACE_FX"] = {"表面特效", "Surface Effects", "显示与图形"};
    m["INFLIGHT_HIGHLIGHT"] = {"飞行中高亮", "In-Flight Highlight", "显示与图形"};
    m["COMET_REENTRY_FRAGMENT"] = {"彗星再入碎片", "Comet Reentry Fragment", "显示与图形"};
    m["REFLECTION_PROBE_REFRESH_MODE"] = {"反射探针刷新模式", "Reflection Probe Refresh Mode", "显示与图形"};
    m["REFLECTION_PROBE_TEXTURE_RESOLUTION"] = {"反射纹理分辨率", "Reflection Texture Resolution", "显示与图形"};
    m["TERRAIN_SHADER_QUALITY"] = {"地形着色器质量", "Terrain Shader Quality", "显示与图形"};
    m["FALLBACK_UNDERWATER_MODE"] = {"水下渲染回退模式", "Underwater Rendering Fallback", "显示与图形"};
    m["SCREENSHOT_SUPERSIZE"] = {"截图超采样", "Screenshot Supersize", "显示与图形"};
    m["CELESTIAL_BODIES_CAST_SHADOWS"] = {"天体投射阴影", "Celestial Bodies Cast Shadows", "显示与图形"};
    m["HIGHLIGHT_FX"] = {"高亮特效", "Highlight Effects", "显示与图形"};
    m["COMET_SHOW_GEYSERS"] = {"显示彗星喷泉", "Show Comet Geysers", "显示与图形"};
    m["COMET_MAXIMUM_GEYSERS"] = {"彗星喷泉最大数量", "Max Comet Geysers", "显示与图形"};
    m["COMET_SHOW_NEAR_DUST"] = {"显示彗星近尘埃", "Show Comet Near Dust", "显示与图形"};
    m["COMET_MAXIMUM_NEAR_DUST_EMITTERS"] = {"彗星近尘埃最大发射器数量", "Max Comet Near-Dust Emitters", "显示与图形"};
    m["PART_HIGHLIGHTER_BRIGHTNESSFACTOR"] = {"部件高亮亮度系数", "Part Highlighter Brightness", "显示与图形"};
    m["TEMPERATURE_GAUGES_MODE"] = {"温度计模式", "Temperature Gauge Mode", "显示与图形"};

    // ===== 音频 (Audio) =====
    m["MASTER_VOLUME"] = {"主音量", "Master Volume", "音频"};
    m["SHIP_VOLUME"] = {"飞船音量", "Ship Volume", "音频"};
    m["AMBIENCE_VOLUME"] = {"环境音量", "Ambience Volume", "音频"};
    m["MUSIC_VOLUME"] = {"音乐音量", "Music Volume", "音频"};
    m["UI_VOLUME"] = {"界面音量", "UI Volume", "音频"};
    m["VOICE_VOLUME"] = {"语音音量", "Voice Volume", "音频"};
    m["SOUND_NORMALIZER_ENABLED"] = {"音频标准化", "Audio Normalizer", "音频"};
    m["SOUND_NORMALIZER_THRESHOLD"] = {"标准化阈值", "Normalizer Threshold", "音频"};
    m["SOUND_NORMALIZER_RESPONSIVENESS"] = {"标准化响应速度", "Normalizer Responsiveness", "音频"};
    m["SOUND_NORMALIZER_SKIPSAMPLES"] = {"标准化跳过采样数", "Normalizer Skip Samples", "音频"};

    // ===== 游戏玩法 (Gameplay) =====
    m["LANGUAGE"] = {"游戏语言", "Game Language", "游戏玩法"};
    m["KERBIN_TIME"] = {"使用Kerbin时间", "Use Kerbin Time", "游戏玩法"};
    m["MAX_VESSELS_BUDGET"] = {"最大飞船数量", "Max Vessels", "游戏玩法"};
    m["SIMULATE_IN_BACKGROUND"] = {"后台模拟", "Simulate in Background", "游戏玩法"};
    m["PHYSICS_FRAME_DT_LIMIT"] = {"物理帧时间限制", "Physics Frame Time Limit", "游戏玩法"};
    m["PHYSICS_EASE"] = {"物理过渡", "Physics Easing", "游戏玩法"};
    m["DECLUTTER_KSC"] = {"KSC界面简化", "Reduce KSC Clutter", "游戏玩法"};
    m["DEFAULT_KERBAL_RESPAWN_TIMER"] = {"小绿人重生计时", "Kerbal Respawn Timer", "游戏玩法"};
    m["SHOW_DEADLINES_AS_DATES"] = {"截止日期显示为日期", "Show Deadlines as Dates", "游戏玩法"};
    m["CAN_ALWAYS_QUICKSAVE"] = {"始终允许快速存档", "Always Allow Quicksave", "游戏玩法"};
    m["QUICKSAVE_MINIMUM_ALTITUDE"] = {"快速存档最低高度", "Quicksave Minimum Altitude", "游戏玩法"};
    m["AUTOSAVE_INTERVAL"] = {"自动存档间隔", "Autosave Interval", "游戏玩法"};
    m["AUTOSAVE_SHORT_INTERVAL"] = {"短自动存档间隔", "Short Autosave Interval", "游戏玩法"};
    m["SAVE_BACKUPS"] = {"存档备份数", "Save Backups", "游戏玩法"};
    m["SHOW_SPACE_CENTER_CREW"] = {"显示航天中心人员", "Show Space Center Crew", "游戏玩法"};
    m["PRELAUNCH_DEFAULT_THROTTLE"] = {"发射前默认油门", "Default Pre-Launch Throttle", "游戏玩法"};
    m["MIN_DISTANCE_FROM_OTHER_SPLASHES"] = {"溅落最小间距", "Min Distance from Other Splashes", "游戏玩法"};
    m["MIN_TIME_BETWEEN_SPLASHES"] = {"溅落最小间隔时间", "Min Time Between Splashes", "游戏玩法"};
    m["ADDITIONAL_ACTION_GROUPS"] = {"额外动作组", "Additional Action Groups", "游戏玩法"};
    m["ADVANCED_TWEAKABLES"] = {"高级调整选项", "Advanced Tweakables", "游戏玩法"};
    m["ADVANCED_MESSAGESAPP"] = {"高级消息应用", "Advanced Messages App", "游戏玩法"};
    m["CONFIRM_MESSAGE_DELETION"] = {"确认删除消息", "Confirm Message Deletion", "游戏玩法"};
    m["AUTOSTRUT_SYMMETRY"] = {"对称自动支撑", "Auto-Strut Symmetry", "游戏玩法"};
    m["EXTENDED_BURNTIME"] = {"扩展燃烧时间", "Extended Burn Time", "游戏玩法"};
    m["SHOW_EXIT_TO_MENU_CONFIRMATION"] = {"退出到菜单确认", "Exit-to-Menu Confirmation", "游戏玩法"};
    m["SHOW_WRONG_VESSEL_TYPE_CONFIRMATION"] = {"错误飞船类型确认", "Wrong Vessel Type Confirmation", "游戏玩法"};
    m["SHOW_VERSION_WATERMARK"] = {"显示版本水印", "Show Version Watermark", "游戏玩法"};
    m["SHOW_ANALYTICS_DIALOG"] = {"显示分析对话框", "Show Analytics Dialog", "游戏玩法"};
    m["SHOW_WHATSNEW_DIALOG"] = {"显示更新内容对话框", "Show What's New Dialog", "游戏玩法"};
    m["SHOW_WHATSNEW_DIALOG_VersionsShown"] = {"已显示更新版本", "What's New Versions Shown", "游戏玩法"};
    m["CALL_HOME_PROMPT"] = {"回传数据提示", "Call-Home Prompt", "游戏玩法"};
    m["DONT_SEND_IP"] = {"不发送IP", "Don't Send IP", "游戏玩法"};
    m["SEND_PROGRESS_DATA"] = {"发送进度数据", "Send Progress Data", "游戏玩法"};
    m["CHECK_FOR_UPDATES"] = {"检查更新", "Check for Updates", "游戏玩法"};
    m["VERBOSE_DEBUG_LOG"] = {"详细调试日志", "Verbose Debug Log", "游戏玩法"};
    m["SHOW_CONSOLE_ON_ERROR"] = {"错误时显示控制台", "Show Console on Error", "游戏玩法"};
    m["CONSOLE_BUFFER_SIZE"] = {"控制台缓冲区大小", "Console Buffer Size", "游戏玩法"};
    m["INPUT_KEYBOARD_SENSIVITITY"] = {"键盘灵敏度", "Keyboard Sensitivity", "游戏玩法"};
    m["TRACKIR_ENABLED"] = {"TrackIR启用", "TrackIR Enabled", "游戏玩法"};
    m["CURRENT_LAYOUT_SETTINGS"] = {"当前按键布局", "Current Key Layout", "游戏玩法"};
    m["AxisSensitivityMin"] = {"轴灵敏度最小值", "Axis Sensitivity Min", "游戏玩法"};
    m["AxisSensitivityMax"] = {"轴灵敏度最大值", "Axis Sensitivity Max", "游戏玩法"};
    m["AXIS_INCREMENTAL_SPEED_MULTIPLIER_STORAGE"] = {"轴增量速度倍率存储", "Axis Incremental Speed (Stored)", "游戏玩法"};
    m["AXIS_INCREMENTAL_SPEED_MULTIPLIER_DEFAULT"] = {"轴增量速度默认倍率", "Axis Incremental Speed (Default)", "游戏玩法"};
    m["AXIS_INCREMENTAL_SPEED_MULTIPLIER_VALUES"] = {"轴增量速度倍率值", "Axis Incremental Speed Values", "游戏玩法"};
    m["dontShowLauncher"] = {"不显示启动器", "Don't Show Launcher", "游戏玩法"};

    // ===== 编辑器(VAB/SPH) (Editor) =====
    m["VAB_USE_CLICK_PLACE"] = {"点击放置", "Click-and-Place", "编辑器"};
    m["VAB_USE_ANGLE_SNAP"] = {"角度对齐", "Angle Snap", "编辑器"};
    m["VAB_ANGLE_SNAP_INCLUDE_VERTICAL"] = {"角度对齐含垂直", "Angle Snap Includes Vertical", "编辑器"};
    m["VAB_FINE_OFFSET_THRESHOLD"] = {"精细偏移阈值", "Fine Offset Threshold", "编辑器"};
    m["VAB_CAMERA_ORBIT_SENS"] = {"VAB相机旋转灵敏度", "VAB Camera Orbit Sensitivity", "编辑器"};
    m["VAB_CAMERA_ZOOM_SENS"] = {"VAB相机缩放灵敏度", "VAB Camera Zoom Sensitivity", "编辑器"};
    m["VAB_CRAFTNAME_CHAR_LIMIT"] = {"飞船名称字符限制", "Craft Name Character Limit", "编辑器"};
    m["EDITOR_UNDO_REDO_LIMIT"] = {"编辑器撤销/重做限制", "Editor Undo/Redo Limit", "编辑器"};
    m["SPACENAV_CAMERA_SENS_ROT"] = {"SpaceNav相机旋转灵敏度", "SpaceNav Camera Rot. Sensitivity", "编辑器"};
    m["SPACENAV_CAMERA_SENS_LIN"] = {"SpaceNav相机线性灵敏度", "SpaceNav Camera Linear Sensitivity", "编辑器"};
    m["SPACENAV_CAMERA_SHARPNESS_LIN"] = {"SpaceNav相机线性锐度", "SpaceNav Camera Linear Sharpness", "编辑器"};
    m["SPACENAV_CAMERA_SHARPNESS_ROT"] = {"SpaceNav相机旋转锐度", "SpaceNav Camera Rot. Sharpness", "编辑器"};
    m["SPACENAV_FLIGHT_SENS_ROT"] = {"SpaceNav飞行旋转灵敏度", "SpaceNav Flight Rot. Sensitivity", "编辑器"};
    m["SPACENAV_FLIGHT_SENS_LIN"] = {"SpaceNav飞行线性灵敏度", "SpaceNav Flight Linear Sensitivity", "编辑器"};
    m["CONTROLPOINT_VISUALS_ENABLED"] = {"控制点可视化", "Control Point Visuals", "编辑器", true};
    m["CONTROLPOINT_ARROWLENGTH"] = {"控制点箭头长度", "Control Point Arrow Length", "编辑器", true};
    m["CONTROLPOINT_COLOR_FORWARD"] = {"控制点前方颜色", "Control Point Forward Color", "编辑器", true};
    m["CONTROLPOINT_COLOR_UP"] = {"控制点上方颜色", "Control Point Up Color", "编辑器", true};
    m["CONTROLPOINT_COLOR_RIGHT"] = {"控制点右方颜色", "Control Point Right Color", "编辑器", true};
    m["STAGE_GROUP_INFO_ITEMS"] = {"分级信息项", "Staging Info Items", "编辑器"};
    m["STAGE_GROUP_INFO_WIDTH_EDITOR"] = {"编辑器分级信息宽度", "Staging Info Width (Editor)", "编辑器"};
    m["STAGE_GROUP_INFO_WIDTH_FLIGHT"] = {"飞行分级信息宽度", "Staging Info Width (Flight)", "编辑器"};
    m["STAGE_GROUP_INFO_NAME_PERCENTAGE"] = {"分级名称占比", "Staging Name Percentage", "编辑器"};
    m["CRAFT_STEAM_UNSUBSCRIBE_WARNING"] = {"Steam取消订阅警告", "Steam Unsubscribe Warning", "编辑器"};

    // ===== 相机 (Camera) =====
    m["FLT_CAMERA_ORBIT_SENS"] = {"飞行相机旋转灵敏度", "Flight Camera Orbit Sensitivity", "相机"};
    m["FLT_CAMERA_ZOOM_SENS"] = {"飞行相机缩放灵敏度", "Flight Camera Zoom Sensitivity", "相机"};
    m["FLT_CAMERA_WOBBLE"] = {"飞行相机抖动", "Flight Camera Wobble", "相机"};
    m["FLT_CAMERA_CHASE_SHARPNESS"] = {"追踪相机锐度", "Chase Camera Sharpness", "相机"};
    m["FLT_CAMERA_CHASE_USEVELOCITYVECTOR"] = {"追踪相机使用速度向量", "Chase Camera Uses Velocity Vector", "相机"};
    m["FLT_VESSEL_LABELS"] = {"飞船标签", "Vessel Labels", "相机"};
    m["CAMERA_DOUBLECLICK_MOUSELOOK"] = {"双击鼠标视角", "Double-Click Mouse Look", "相机"};
    m["DOUBLECLICK_MOUSESPEED"] = {"双击鼠标速度", "Double-Click Mouse Speed", "相机"};
    m["IVA_RETAIN_CONTROL_POINT"] = {"IVA保持控制点", "IVA Retain Control Point", "相机"};
    m["CAMERA_FX_EXTERNAL"] = {"外部相机特效", "External Camera Effects", "相机"};
    m["CAMERA_FX_INTERNAL"] = {"内部相机特效", "Internal Camera Effects", "相机"};

    // ===== EVA/舱外活动 (EVA) =====
    m["EVA_ROTATE_ON_MOVE"] = {"移动时旋转", "Rotate on Move", "EVA"};
    m["EVA_SHOW_PORTRAIT"] = {"显示肖像", "Show Portrait", "EVA"};
    m["EVA_DEFAULT_HELMET_ON"] = {"默认头盔开启", "Default Helmet On", "EVA"};
    m["EVA_DEFAULT_NECKRING_ON"] = {"默认颈环开启", "Default Neck Ring On", "EVA"};
    m["EVA_DIES_WHEN_UNSAFE_HELMET"] = {"不安全时摘头盔死亡", "Dies When Helmet Unsafe", "EVA"};
    m["EVA_INHERIT_PART_TEMPERATURE"] = {"继承部件温度", "Inherit Part Temperature", "EVA"};
    m["EVA_SCREEN_MESSAGE_X"] = {"EVA消息X坐标", "EVA Message X Position", "EVA"};
    m["EVA_SCREEN_MESSAGE_Y"] = {"EVA消息Y坐标", "EVA Message Y Position", "EVA"};
    m["EVA_LADDER_CHECK_END"] = {"梯子末端检查", "Ladder End Check", "EVA"};
    m["EVA_LADDER_JOINT_WHEN_IDLE"] = {"闲置时梯子关节", "Ladder Joint When Idle", "EVA"};
    m["EVA_LADDER_JOINT_BREAK_VELOCITY"] = {"梯子关节断裂速度", "Ladder Joint Break Velocity", "EVA"};
    m["EVA_LADDER_JOINT_BREAK_ACCELERATION"] = {"梯子关节断裂加速度", "Ladder Joint Break Acceleration", "EVA"};
    m["EVA_MAX_SLOPE_ANGLE"] = {"最大坡度角", "Max Slope Angle", "EVA"};
    m["EVA_INVENTORY_RANGE"] = {"库存范围", "Inventory Range", "EVA"};
    m["EVA_CONSTRUCTION_RANGE"] = {"建造范围", "Construction Range", "EVA"};
    m["EVA_CONSTRUCTION_COMBINE_ENABLED"] = {"启用合并建造", "Enable Construction Combining", "EVA"};
    m["EVA_CONSTRUCTION_COMBINE_NONENGINEERS"] = {"非工程师合并建造", "Non-Engineer Combiners", "EVA"};
    m["EVA_CONSTRUCTION_COMBINE_RANGE"] = {"合并建造范围", "Combine Construction Range", "EVA"};
    m["PART_REPAIR_MASS_PER_KIT"] = {"每套件修复质量", "Repair Mass per Kit", "EVA"};
    m["PART_REPAIR_MAX_KIT_AMOUNT"] = {"最大套件数量", "Max Repair Kits", "EVA"};

    // ===== 轨道 (Orbit) =====
    m["CONIC_PATCH_DRAW_MODE"] = {"轨道绘制模式", "Orbit Draw Mode", "轨道"};
    m["CONIC_PATCH_LIMIT"] = {"轨道显示数量", "Orbit Patch Count", "轨道"};
    m["ALWAYS_SHOW_TARGET_APPROACH_MARKERS"] = {"始终显示目标接近标记", "Always Show Approach Markers", "轨道"};
    m["ORBIT_FADE_STRENGTH"] = {"轨道淡出强度", "Orbit Fade Strength", "轨道"};
    m["ORBIT_FADE_DIRECTION_INV"] = {"轨道淡出方向反转", "Orbit Fade Direction Inverted", "轨道"};
    m["ORBIT_WARP_DOWN_AT_SOI"] = {"SOI处时间加速降速", "Slow Time Warp at SOI", "轨道"};
    m["ORBIT_DRIFT_COMPENSATION"] = {"轨道漂移补偿", "Orbit Drift Compensation", "轨道"};
    m["LEGACY_ORBIT_TARGETING"] = {"旧版轨道瞄准", "Legacy Orbit Targeting", "轨道"};
    m["SHOW_PWARP_WARNING"] = {"物理加速警告", "Physics Warp Warning", "轨道"};
    m["ORBIT_WARP_MAXRATE_MODE"] = {"最大加速模式", "Max Time-Warp Rate Mode", "轨道"};
    m["ORBIT_WARP_PEMODE_SURFACE_MARGIN"] = {"近地点表面余量", "Periapsis Surface Margin", "轨道"};
    m["ORBIT_WARP_ALTMODE_LIMIT_MODIFIER"] = {"高度模式限制修饰", "Altitude Mode Limit Modifier", "轨道"};
    m["RADAR_ALTIMETER_EXTENDED_CALCS"] = {"雷达高度计扩展计算", "Radar Altimeter Extended Calcs", "轨道"};
    m["MAP_MAX_ORBIT_BEFORE_FORCE2D"] = {"强制2D前最大轨道数", "Max Orbits Before Forced 2D", "轨道", true};

    // ===== 界面(UI) (UI) =====
    m["UI_SCALE"] = {"界面缩放", "UI Scale", "界面"};
    m["UI_OPACITY"] = {"界面不透明度", "UI Opacity", "界面"};
    m["UI_MAINCANVAS_PIXEL_PERFECT"] = {"主画布像素对齐", "Main Canvas Pixel-Perfect", "界面"};
    m["UI_ACTIONCANVAS_PIXEL_PERFECT"] = {"动作画布像素对齐", "Action Canvas Pixel-Perfect", "界面"};
    m["UI_TOOLTIPCANVAS_PIXEL_PERFECT"] = {"提示画布像素对齐", "Tooltip Canvas Pixel-Perfect", "界面"};
    m["AUTOHIDE_NAVBALL"] = {"自动隐藏导航球", "Auto-Hide Navball", "界面"};
    m["WARP_TO_MANNODE_MARGIN"] = {"加速到机动节点余量", "Warp-to-Maneuver Margin", "界面"};
    m["UIELEMENTSCALINGENABLED"] = {"界面元素缩放启用", "UI Element Scaling Enabled", "界面"};
    m["UI_SCALE_TIME"] = {"时间缩放", "Time Scale", "界面"};
    m["UI_SCALE_ALTIMETER"] = {"高度计缩放", "Altimeter Scale", "界面"};
    m["UI_SCALE_MAPOPTIONS"] = {"地图选项缩放", "Map Options Scale", "界面"};
    m["UI_SCALE_APPS"] = {"应用缩放", "Apps Scale", "界面"};
    m["UI_SCALE_STAGINGSTACK"] = {"分级堆栈缩放", "Staging Stack Scale", "界面"};
    m["UI_SCALE_MODE"] = {"模式缩放", "Mode Scale", "界面"};
    m["UI_SCALE_NAVBALL"] = {"导航球缩放", "Navball Scale", "界面"};
    m["UI_SCALE_CREW"] = {"船员缩放", "Crew Scale", "界面"};
    m["UI_POS_NAVBALL"] = {"导航球位置", "Navball Position", "界面"};
    m["UI_COLOR_INACTIVE_TEXT"] = {"非激活文本颜色", "Inactive Text Color", "界面", true};
    m["UI_COLOR_ACTIVE_TEXT"] = {"激活文本颜色", "Active Text Color", "界面", true};
    m["UI_COLOR_INACTIVE_MINISETTNIGS_TEXT"] = {"非激活迷你设置文本颜色", "Inactive Mini-Settings Text Color", "界面", true};
    m["UI_POS_ALTIMETER_SLIDEDOWN_HOVER_HEIGHT"] = {"高度计悬停下滑高度", "Altimeter Hover Height", "界面"};
    m["MAPNODE_BEHINDBODY_OPACITY"] = {"天体后方节点不透明度", "Node Opacity Behind Body", "界面"};
    m["COMMNET_LOWCOLOR_BRIGHTNESSFACTOR"] = {"通讯网络低亮度系数", "CommNet Low-Color Brightness", "界面"};
    m["SHOW_VESSEL_NAMING_IN_FLIGHT"] = {"飞行中显示飞船命名", "Show Vessel Naming in Flight", "界面"};
    m["VESSEL_NAMING_PRIORTY_LEVEL_MAX"] = {"飞船命名最大优先级", "Vessel Naming Max Priority", "界面"};
    m["VESSEL_NAMING_PRIORTY_LEVEL_DEFAULT"] = {"飞船命名默认优先级", "Vessel Naming Default Priority", "界面"};
    m["SCIENCE_EXPERIMENT_SHOW_TRANSFER_WARNING"] = {"科学实验传输警告", "Experiment Transfer Warning", "界面"};
    m["NAVIGATION_GHOSTING"] = {"导航虚影", "Navigation Ghosting", "界面"};
    m["VESSEL_ANCHOR_VELOCITY_THRESHOLD"] = {"飞船锚定速度阈值", "Anchor Velocity Threshold", "界面"};
    m["VESSEL_ANCHOR_TIME_THRESHOLD"] = {"飞船锚定时间阈值", "Anchor Time Threshold", "界面"};
    m["VESSEL_ANCHOR_ANGLE_CHANGE_THRESHOLD"] = {"飞船锚定角度变化阈值", "Anchor Angle-Change Threshold", "界面"};
    m["VESSEL_ANCHOR_ANGLE_TIME_THRESHOLD"] = {"飞船锚定角度时间阈值", "Anchor Angle-Time Threshold", "界面"};
    m["VESSEL_ANCHOR_BREAK_FORCE_FACTOR"] = {"飞船锚定断裂力系数", "Anchor Break Force Factor", "界面"};
    m["VESSEL_ANCHOR_BREAK_TORQUE"] = {"飞船锚定断裂扭矩", "Anchor Break Torque", "界面"};
    m["PAW_COLLAPSED_GROUP_NAMES"] = {"PAW折叠组名", "PAW Collapsed Group Names", "界面", true};
    m["PAW_NUMERIC_SLIDERS"] = {"PAW数字滑块", "PAW Numeric Sliders", "界面", true};
    m["PAW_PREFERRED_HEIGHT"] = {"PAW首选高度", "PAW Preferred Height", "界面", true};
    m["PAW_SCREEN_OFFSET_X"] = {"PAW屏幕X偏移", "PAW Screen X Offset", "界面", true};
    m["SHOW_DELETE_ALARM_CONFIRMATION"] = {"删除闹钟确认", "Confirm Deleting Alarms", "界面"};
    m["ALARM_ROW_DISPLAYED_FLIGHT"] = {"飞行中显示闹钟行", "Show Alarm Row in Flight", "界面"};
    m["SHOW_TRANSLATION_KEYS_ON_SCREEN"] = {"屏幕显示翻译键", "Show Translation Keys on Screen", "界面"};

    // ===== Delta-V =====
    m["DELTAV_CALCULATIONS_ENABLED"] = {"Delta-V计算启用", "Enable Delta-V Calculations", "Delta-V"};
    m["DELTAV_BURN_PERCENTAGE"] = {"燃烧百分比", "Burn Percentage", "Delta-V"};
    m["DELTAV_BURN_ESTIMATE_COLORS"] = {"燃烧估算颜色", "Burn Estimate Colors", "Delta-V"};
    m["DELTAV_BURN_TIME_COLORS"] = {"燃烧时间颜色", "Burn Time Colors", "Delta-V"};
    m["DELTAV_ACTIVE_STAGE_UPDATE_SECS"] = {"激活级更新间隔", "Active Stage Update Interval", "Delta-V"};
    m["DELTAV_ALL_STAGES_UPDATE_SECS"] = {"所有级更新间隔", "All Stages Update Interval", "Delta-V"};
    m["DELTAV_VESSEL_EVENT_DELAY_SECS"] = {"飞船事件延迟", "Vessel Event Delay", "Delta-V"};
    m["DELTAV_ACTIVE_VESSEL_TIMESTEP"] = {"激活飞船时间步", "Active Vessel Timestep", "Delta-V"};
    m["DELTAV_CALCULATIONS_TIMESTEP"] = {"计算时间步", "Calculation Timestep", "Delta-V"};
    m["DELTAV_CALCULATIONS_BIGTIMESTEP"] = {"计算大时间步", "Large Calculation Timestep", "Delta-V"};
    m["DELTAV_USE_TIMED_VESSELCALCS"] = {"使用定时飞船计算", "Use Timed Vessel Calculations", "Delta-V"};
    m["DELTAV_APP_ENABLED"] = {"Delta-V应用启用", "Enable Delta-V App", "Delta-V"};
    m["DELTAV_APP_TWOCOLUMN_MODE"] = {"双列模式", "Two-Column Mode", "Delta-V"};
    m["LOG_DELTAV_VERBOSE"] = {"详细Delta-V日志", "Verbose Delta-V Log", "Delta-V"};

    // ===== 车轮 (Wheels) =====
    m["WHEEL_WEIGHT_STRESS_MULTIPLIER"] = {"车轮重量压力倍率", "Wheel Weight Stress Multiplier", "车轮"};
    m["WHEEL_SLIP_STRESS_MULTIPLIER"] = {"车轮滑动压力倍率", "Wheel Slip Stress Multiplier", "车轮"};
    m["WHEEL_SUBSTEPS_ACTIVE"] = {"车轮激活子步数", "Active Wheel Substeps", "车轮"};
    m["WHEEL_SUBSTEPS_INACTIVE"] = {"车轮非激活子步数", "Inactive Wheel Substeps", "车轮"};
    m["WHEEL_AUTO_SPRINGDAMPER"] = {"自动弹簧阻尼", "Auto Spring Damper", "车轮"};
    m["WHEEL_AUTO_STEERINGADJUST"] = {"自动转向调整", "Auto Steering Adjust", "车轮"};
    m["LEGS_ADVANCED_SUSPENSIONDAMPER"] = {"着陆腿高级悬挂阻尼", "Leg Adv. Suspension Damper", "车轮"};
    m["WHEEL_DAMAGE_IMPACTCOLLIDER_ENABLED"] = {"碰撞冲击损坏", "Damage from Impact", "车轮"};
    m["WHEEL_DAMAGE_WHEELCOLLIDER_ENABLED"] = {"车轮碰撞损坏", "Damage from Wheel Collisions", "车轮"};

    // ===== KSPNet/通讯 (KerbNet) =====
    m["KERBNET_ALIGNS_WITH_ORBIT"] = {"KerbNet与轨道对齐", "KerbNet Aligns with Orbit", "KerbNet"};
    m["KERBNET_REFRESH_FAST_INTERVAL"] = {"KerbNet快速刷新间隔", "KerbNet Fast Refresh Interval", "KerbNet"};
    m["KERBNET_REFRESH_SLOW_INTERVAL"] = {"KerbNet慢速刷新间隔", "KerbNet Slow Refresh Interval", "KerbNet"};
    m["KERBNET_BACKGROUND_FLUFF"] = {"KerbNet后台填充", "KerbNet Background Fluff", "KerbNet"};

    // ===== 任务编辑器 (Mission Editor) =====
    m["MISSION_SHOW_CREATE_VESSEL_WARNING"] = {"创建飞船警告", "Create-Vessel Warning", "任务编辑器"};
    m["MISSION_SHOW_TEST_MISSION_WARNING"] = {"测试任务警告", "Test-Mission Warning", "任务编辑器"};
    m["MISSION_SHOW_NO_BRIEFING_WARNING"] = {"无简报警告", "No-Briefing Warning", "任务编辑器"};
    m["MISSION_STEAM_UNSUBSCRIBE_WARNING"] = {"任务Steam取消订阅警告", "Mission Steam Unsubscribe Warning", "任务编辑器"};
    m["MISSION_SNAP_TO_GRID"] = {"对齐网格", "Snap to Grid", "任务编辑器"};
    m["MISSION_BUILDER_GAPHEIGHT"] = {"构建器间隙高度", "Builder Gap Height", "任务编辑器"};
    m["MISSION_SHOW_STOCK_PACKS_IN_BRIEFING"] = {"简报显示原版包", "Show Stock Packs in Briefing", "任务编辑器"};
    m["MISSION_LOG_NODE_ACTIVATIONS"] = {"记录节点激活", "Log Node Activations", "任务编辑器"};
    m["MISSION_SHOW_EXPANSION_INFO"] = {"显示扩展信息", "Show Expansion Info", "任务编辑器"};
    m["MISSION_MINIMUM_CANVAS_ZOOM"] = {"最小画布缩放", "Minimum Canvas Zoom", "任务编辑器"};
    m["MISSION_GAP_CAMERA_VAB_CONTROLS"] = {"间隙相机VAB控制", "Gap Camera VAB Controls", "任务编辑器"};
    m["MISSION_DELETE_REMOVES_IN_PROGRESS_MISSIONS"] = {"删除移除进行中任务", "Deletion Removes In-Progress Missions", "任务编辑器"};
    m["MISSION_NAVIGATION_GHOSTING"] = {"任务导航虚影", "Mission Navigation Ghosting", "任务编辑器"};
    m["MISSION_VALIDATOR_MODE"] = {"任务验证模式", "Validator Mode", "任务编辑器"};
    m["MISSION_TEST_AUTOMATIC_CHECKPOINTS"] = {"自动检查点测试", "Test Automatic Checkpoints", "任务编辑器"};
    m["MANEUVER_TOOL_TRANSFER_DEGREES"] = {"机动工具转移角度", "Maneuver Transfer Angles", "任务编辑器"};
    m["MANEUVER_TOOL_CALC_TIMEOUT"] = {"机动工具计算超时", "Maneuver Calculation Timeout", "任务编辑器"};
    m["MANEUVER_TOOL_CB_COLLISION_ADJUSTMENT"] = {"天体碰撞调整", "Celestial-Body Collision Adj.", "任务编辑器"};

    // ===== Breaking Ground =====
    m["SERENITY_SHOW_EXPANSION_INFO"] = {"Breaking Ground扩展信息", "Breaking Ground Expansion Info", "Breaking Ground"};
    m["SERENITY_ROCS_VISUAL_SPEED"] = {"Breaking Ground ROC视觉速度", "Breaking Ground ROC Visual Speed", "Breaking Ground"};
    m["SERENITY_CONTROLLER_IGNORES_VESSEL"] = {"Breaking Ground控制器忽略飞船", "Breaking Ground Controller Ignores Vessel", "Breaking Ground"};

    // ===== 教程 (Tutorials) =====
    m["TUTORIALS_EDITOR_ENABLE"] = {"编辑器教程启用", "Editor Tutorial Enabled", "教程"};
    m["TUTORIALS_FLIGHT_ENABLE"] = {"飞行教程启用", "Flight Tutorial Enabled", "教程"};
    m["TUTORIALS_MISSION_SCREEN_TUTORIAL_COMPLETED"] = {"任务界面教程已完成", "Mission Tutorial Completed", "教程"};
    m["TUTORIALS_MISSION_BUILDER_ENTERED"] = {"进入任务构建器", "Mission Builder Entered", "教程"};
    m["TUTORIALS_ESA_MISSION_SCREEN_TUTORIAL_COMPLETED"] = {"ESA任务教程已完成", "ESA Mission Tutorial Completed", "教程"};

    // ===== 调试与日志 (Debug & Log) =====
    m["LOG_INSTANT_FLUSH"] = {"日志立即刷新", "Instant Log Flush", "调试与日志"};
    m["LOG_ERRORS_TO_SCREEN"] = {"错误显示到屏幕", "Show Errors on Screen", "调试与日志"};
    m["LOG_EXCEPTIONS_TO_SCREEN"] = {"异常显示到屏幕", "Show Exceptions on Screen", "调试与日志"};
    m["LOG_JOINT_BREAK_EVENT"] = {"记录关节断裂事件", "Log Joint-Break Events", "调试与日志"};
    m["LOG_MISSING_KEYS_TO_FILE"] = {"缺失键写入文件", "Log Missing Keys to File", "调试与日志"};
    m["LOG_FXMONGER_VERBOSE"] = {"详细FXMonger日志", "Verbose FXMonger Log", "调试与日志"};
    m["COLLECT_ROC_STATS"] = {"收集ROC统计", "Collect ROC Stats", "调试与日志"};
    m["DEBUG_MAX_SETPOSITION_ALTITUDE"] = {"最大SetPosition高度", "Max SetPosition Altitude", "调试与日志"};
    m["DEBUG_AERO_GUI"] = {"气动调试界面", "Aerodynamics Debug UI", "调试与日志"};
    m["DEBUG_AERO_DATA_PAWS"] = {"气动数据PAW", "Aerodynamics Data PAW", "调试与日志"};
    m["FI_LOG_TEMP_ERROR"] = {"FI临时错误日志", "FlightIntegrator Temp Error Log", "调试与日志"};
    m["FI_LOG_OVERTEMP"] = {"FI过热日志", "FlightIntegrator Overheat Log", "调试与日志"};

    // ===== 颜色设置 (Colors) =====
    m["COLOR_PART_HIGHLIGHT"] = {"部件高亮颜色", "Part Highlight Color", "颜色设置", true};
    m["COLOR_PART_EDITORATTACHED"] = {"编辑器已附着颜色", "Editor Attached Color", "颜色设置", true};
    m["COLOR_PART_EDITORDETACHED"] = {"编辑器已分离颜色", "Editor Detached Color", "颜色设置", true};
    m["COLOR_PART_ACTIONGROUP_SELECTED"] = {"动作组选中颜色", "Action Group Selected Color", "颜色设置", true};
    m["COLOR_PART_ACTIONGROUP_HIGHLIGHT"] = {"动作组高亮颜色", "Action Group Highlight Color", "颜色设置", true};
    m["COLOR_PART_ROOTTOOL_HIGHLIGHT"] = {"根工具高亮颜色", "Root Tool Highlight Color", "颜色设置", true};
    m["COLOR_PART_ROOTTOOL_HIGHLIGHTEDGE"] = {"根工具高亮边缘颜色", "Root Tool Highlight Edge Color", "颜色设置", true};
    m["COLOR_PART_ROOTTOOL_HOVER"] = {"根工具悬停颜色", "Root Tool Hover Color", "颜色设置", true};
    m["COLOR_PART_ROOTTOOL_HOVEREDGE"] = {"根工具悬停边缘颜色", "Root Tool Hover Edge Color", "颜色设置", true};
    m["COLOR_PART_ENGINEERAPP_HIGHLIGHT"] = {"工程师应用高亮颜色", "Engineer App Highlight Color", "颜色设置", true};
    m["COLOR_PART_TRANSFER_SOURCE_HIGHLIGHT"] = {"传输源高亮颜色", "Transfer Source Highlight Color", "颜色设置", true};
    m["COLOR_PART_TRANSFER_SOURCE_HOVER"] = {"传输源悬停颜色", "Transfer Source Hover Color", "颜色设置", true};
    m["COLOR_PART_TRANSFER_DEST_HIGHLIGHT"] = {"传输目标高亮颜色", "Transfer Dest. Highlight Color", "颜色设置", true};
    m["COLOR_PART_TRANSFER_DEST_HOVER"] = {"传输目标悬停颜色", "Transfer Dest. Hover Color", "颜色设置", true};
    m["COLOR_PART_INVENTORY_CONTAINER"] = {"库存容器颜色", "Inventory Container Color", "颜色设置", true};
    m["COLOR_PART_INVENTORY_NOSPACE"] = {"库存无空间颜色", "Inventory No-Space Color", "颜色设置", true};
    m["COLOR_PART_CONSTRUCTION_VALID"] = {"建造有效颜色", "Construction Valid Color", "颜色设置", true};
    m["COLOR_RD_SEARCH_NODE_HIGHLIGHT"] = {"RD搜索节点高亮", "R&D Search Node Highlight", "颜色设置", true};
    m["COLOR_RD_SEARCH_PART_HIGHLIGHT"] = {"RD搜索部件高亮", "R&D Search Part Highlight", "颜色设置", true};
    m["COLOR_LIGHT_PRESET_1"] = {"灯光预设1", "Light Preset 1", "颜色设置", true};
    m["COLOR_LIGHT_PRESET_2"] = {"灯光预设2", "Light Preset 2", "颜色设置", true};
    m["COLOR_LIGHT_PRESET_3"] = {"灯光预设3", "Light Preset 3", "颜色设置", true};
    m["COLOR_LIGHT_PRESET_4"] = {"灯光预设4", "Light Preset 4", "颜色设置", true};
    m["COLOR_LIGHT_PRESET_5"] = {"灯光预设5", "Light Preset 5", "颜色设置", true};
    m["COLOR_FIREWORK_PRESET_GREEN"] = {"烟花预设-绿", "Firework Preset - Green", "颜色设置", true};
    m["COLOR_FIREWORK_PRESET_LIGHT_BLUE"] = {"烟花预设-浅蓝", "Firework Preset - Light Blue", "颜色设置", true};
    m["COLOR_FIREWORK_PRESET_BLUE"] = {"烟花预设-蓝", "Firework Preset - Blue", "颜色设置", true};
    m["COLOR_FIREWORK_PRESET_PURPLE"] = {"烟花预设-紫", "Firework Preset - Purple", "颜色设置", true};
    m["COLOR_FIREWORK_PRESET_PINK"] = {"烟花预设-粉", "Firework Preset - Pink", "颜色设置", true};

    // ===== 其他 (Other) =====
    m["FEMALE_EYE_OFFSET_X"] = {"女性眼睛X偏移", "Female Eye X Offset", "其他", true};
    m["FEMALE_EYE_OFFSET_Y"] = {"女性眼睛Y偏移", "Female Eye Y Offset", "其他", true};
    m["FEMALE_EYE_OFFSET_Z"] = {"女性眼睛Z偏移", "Female Eye Z Offset", "其他", true};
    m["FEMALE_EYE_OFFSET_SCALE"] = {"女性眼睛偏移缩放", "Female Eye Offset Scale", "其他", true};

    return m;
}

} // namespace

InstanceKeyInfo instanceGetKeyInfo(const QString& key) {
    static QMap<QString, KeyInfo> info = createKeyInfoMap();
    auto it = info.find(key);
    if (it != info.end()) {
        if (ConfigManager::instance().language() == "en_US") {
            InstanceKeyInfo en;
            en.displayName = it.value().displayNameEn;
            en.category = it.value().category;
            en.hidden = it.value().hidden;
            // Translate category names to English
            static const QMap<QString, QString> catMap = {
                {"显示与图形", "Display & Graphics"},
                {"音频", "Audio"},
                {"游戏玩法", "Gameplay"},
                {"编辑器", "Editor"},
                {"相机", "Camera"},
                {"EVA", "EVA"},
                {"轨道", "Orbit"},
                {"界面", "UI"},
                {"Delta-V", "Delta-V"},
                {"车轮", "Wheels"},
                {"KerbNet", "KerbNet"},
                {"任务编辑器", "Mission Editor"},
                {"Breaking Ground", "Breaking Ground"},
                {"教程", "Tutorials"},
                {"调试与日志", "Debug & Log"},
                {"颜色设置", "Colors"},
                {"其他", "Other"}
            };
            auto catIt = catMap.find(en.category);
            if (catIt != catMap.end()) {
                en.category = catIt.value();
            }
            return en;
        }
        return {it.value().displayName, it.value().category, it.value().hidden};
    }
    return {key, "其他"};
}