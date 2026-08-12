#include "instancemanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QMap>

namespace {

struct KeyInfo {
    QString displayName;
    QString category;
};

QMap<QString, KeyInfo> createKeyInfoMap() {
    QMap<QString, KeyInfo> m;
    // ===== 显示与图形 =====
    m["SCREEN_RESOLUTION_WIDTH"] = {"屏幕宽度", "显示与图形"};
    m["SCREEN_RESOLUTION_HEIGHT"] = {"屏幕高度", "显示与图形"};
    m["FULLSCREEN"] = {"全屏模式", "显示与图形"};
    m["QUALITY_PRESET"] = {"画质预设", "显示与图形"};
    m["ANTI_ALIASING"] = {"抗锯齿", "显示与图形"};
    m["TEXTURE_QUALITY"] = {"纹理质量", "显示与图形"};
    m["SYNC_VBL"] = {"垂直同步(VSync)", "显示与图形"};
    m["LIGHT_QUALITY"] = {"光照质量", "显示与图形"};
    m["SHADOWS_QUALITY"] = {"阴影质量", "显示与图形"};
    m["FRAMERATE_LIMIT"] = {"帧率限制", "显示与图形"};
    m["SHADOWS_FLIGHT_PROJECTION"] = {"飞行阴影投影", "显示与图形"};
    m["SHADOWS_KSC_PROJECTION"] = {"KSC阴影投影", "显示与图形"};
    m["SHADOWS_TRACKING_PROJECTION"] = {"追踪站阴影投影", "显示与图形"};
    m["SHADOWS_EDITORS_PROJECTION"] = {"编辑器阴影投影", "显示与图形"};
    m["SHADOWS_MAIN_PROJECTION"] = {"主菜单阴影投影", "显示与图形"};
    m["SHADOWS_DEFAULT_PROJECTION"] = {"默认阴影投影", "显示与图形"};
    m["AMBIENTLIGHT_BOOSTFACTOR"] = {"环境光增强系数", "显示与图形"};
    m["AMBIENTLIGHT_BOOSTFACTOR_MAPONLY"] = {"地图环境光增强", "显示与图形"};
    m["AMBIENTLIGHT_BOOSTFACTOR_EDITONLY"] = {"编辑器环境光增强", "显示与图形"};
    m["PLANET_SCATTER"] = {"行星表面散布", "显示与图形"};
    m["PLANET_SCATTER_FACTOR"] = {"散布密度系数", "显示与图形"};
    m["WATERLEVEL_BASE_OFFSET"] = {"水位基础偏移", "显示与图形"};
    m["WATERLEVEL_MAXLEVEL_MULT"] = {"水位最大倍率", "显示与图形"};
    m["UNSUPPORTED_LEGACY_SHADER_TERRAIN"] = {"使用旧版地形着色器", "显示与图形"};
    m["AERO_FX_QUALITY"] = {"气动效果质量", "显示与图形"};
    m["SURFACE_FX"] = {"表面特效", "显示与图形"};
    m["INFLIGHT_HIGHLIGHT"] = {"飞行中高亮", "显示与图形"};
    m["COMET_REENTRY_FRAGMENT"] = {"彗星再入碎片", "显示与图形"};
    m["REFLECTION_PROBE_REFRESH_MODE"] = {"反射探针刷新模式", "显示与图形"};
    m["REFLECTION_PROBE_TEXTURE_RESOLUTION"] = {"反射纹理分辨率", "显示与图形"};
    m["TERRAIN_SHADER_QUALITY"] = {"地形着色器质量", "显示与图形"};
    m["FALLBACK_UNDERWATER_MODE"] = {"水下渲染回退模式", "显示与图形"};
    m["SCREENSHOT_SUPERSIZE"] = {"截图超采样", "显示与图形"};
    m["CELESTIAL_BODIES_CAST_SHADOWS"] = {"天体投射阴影", "显示与图形"};
    m["HIGHLIGHT_FX"] = {"高亮特效", "显示与图形"};
    m["COMET_SHOW_GEYSERS"] = {"显示彗星喷泉", "显示与图形"};
    m["COMET_MAXIMUM_GEYSERS"] = {"彗星喷泉最大数量", "显示与图形"};
    m["COMET_SHOW_NEAR_DUST"] = {"显示彗星近尘", "显示与图形"};
    m["COMET_MAXIMUM_NEAR_DUST_EMITTERS"] = {"近尘发射器最大数量", "显示与图形"};
    m["PART_HIGHLIGHTER_BRIGHTNESSFACTOR"] = {"部件高亮亮度系数", "显示与图形"};
    m["TEMPERATURE_GAUGES_MODE"] = {"温度计模式", "显示与图形"};

    // ===== 音频 =====
    m["MASTER_VOLUME"] = {"主音量", "音频"};
    m["SHIP_VOLUME"] = {"飞船音量", "音频"};
    m["AMBIENCE_VOLUME"] = {"环境音量", "音频"};
    m["MUSIC_VOLUME"] = {"音乐音量", "音频"};
    m["UI_VOLUME"] = {"界面音量", "音频"};
    m["VOICE_VOLUME"] = {"语音音量", "音频"};
    m["SOUND_NORMALIZER_ENABLED"] = {"音频标准化", "音频"};
    m["SOUND_NORMALIZER_THRESHOLD"] = {"标准化阈值", "音频"};
    m["SOUND_NORMALIZER_RESPONSIVENESS"] = {"标准化响应速度", "音频"};
    m["SOUND_NORMALIZER_SKIPSAMPLES"] = {"标准化跳过采样数", "音频"};

    // ===== 游戏玩法 =====
    m["LANGUAGE"] = {"游戏语言", "游戏玩法"};
    m["KERBIN_TIME"] = {"使用克尔宾时间", "游戏玩法"};
    m["MAX_VESSELS_BUDGET"] = {"最大飞船数量", "游戏玩法"};
    m["SIMULATE_IN_BACKGROUND"] = {"后台模拟", "游戏玩法"};
    m["PHYSICS_FRAME_DT_LIMIT"] = {"物理帧时间限制", "游戏玩法"};
    m["PHYSICS_EASE"] = {"物理过渡", "游戏玩法"};
    m["DECLUTTER_KSC"] = {"KSC界面简化", "游戏玩法"};
    m["DEFAULT_KERBAL_RESPAWN_TIMER"] = {"坎巴拉人重生计时", "游戏玩法"};
    m["SHOW_DEADLINES_AS_DATES"] = {"截止日期显示为日期", "游戏玩法"};
    m["CAN_ALWAYS_QUICKSAVE"] = {"始终允许快速存档", "游戏玩法"};
    m["QUICKSAVE_MINIMUM_ALTITUDE"] = {"快速存档最低高度", "游戏玩法"};
    m["AUTOSAVE_INTERVAL"] = {"自动存档间隔", "游戏玩法"};
    m["AUTOSAVE_SHORT_INTERVAL"] = {"短自动存档间隔", "游戏玩法"};
    m["SAVE_BACKUPS"] = {"存档备份数", "游戏玩法"};
    m["SHOW_SPACE_CENTER_CREW"] = {"显示航天中心人员", "游戏玩法"};
    m["PRELAUNCH_DEFAULT_THROTTLE"] = {"发射前默认油门", "游戏玩法"};
    m["MIN_DISTANCE_FROM_OTHER_SPLASHES"] = {"溅落最小间距", "游戏玩法"};
    m["MIN_TIME_BETWEEN_SPLASHES"] = {"溅落最小间隔时间", "游戏玩法"};
    m["ADDITIONAL_ACTION_GROUPS"] = {"额外动作组", "游戏玩法"};
    m["ADVANCED_TWEAKABLES"] = {"高级调整选项", "游戏玩法"};
    m["ADVANCED_MESSAGESAPP"] = {"高级消息应用", "游戏玩法"};
    m["CONFIRM_MESSAGE_DELETION"] = {"确认删除消息", "游戏玩法"};
    m["AUTOSTRUT_SYMMETRY"] = {"对称自动支撑", "游戏玩法"};
    m["EXTENDED_BURNTIME"] = {"扩展燃烧时间", "游戏玩法"};
    m["SHOW_EXIT_TO_MENU_CONFIRMATION"] = {"退出到菜单确认", "游戏玩法"};
    m["SHOW_WRONG_VESSEL_TYPE_CONFIRMATION"] = {"错误飞船类型确认", "游戏玩法"};
    m["SHOW_VERSION_WATERMARK"] = {"显示版本水印", "游戏玩法"};
    m["SHOW_ANALYTICS_DIALOG"] = {"显示分析对话框", "游戏玩法"};
    m["SHOW_WHATSNEW_DIALOG"] = {"显示更新内容对话框", "游戏玩法"};
    m["SHOW_WHATSNEW_DIALOG_VersionsShown"] = {"已显示更新版本", "游戏玩法"};
    m["CALL_HOME_PROMPT"] = {"回传数据提示", "游戏玩法"};
    m["DONT_SEND_IP"] = {"不发送IP", "游戏玩法"};
    m["SEND_PROGRESS_DATA"] = {"发送进度数据", "游戏玩法"};
    m["CHECK_FOR_UPDATES"] = {"检查更新", "游戏玩法"};
    m["VERBOSE_DEBUG_LOG"] = {"详细调试日志", "游戏玩法"};
    m["SHOW_CONSOLE_ON_ERROR"] = {"错误时显示控制台", "游戏玩法"};
    m["CONSOLE_BUFFER_SIZE"] = {"控制台缓冲区大小", "游戏玩法"};
    m["INPUT_KEYBOARD_SENSIVITITY"] = {"键盘灵敏度", "游戏玩法"};
    m["TRACKIR_ENABLED"] = {"TrackIR启用", "游戏玩法"};
    m["CURRENT_LAYOUT_SETTINGS"] = {"当前按键布局", "游戏玩法"};
    m["AxisSensitivityMin"] = {"轴灵敏度最小值", "游戏玩法"};
    m["AxisSensitivityMax"] = {"轴灵敏度最大值", "游戏玩法"};
    m["AXIS_INCREMENTAL_SPEED_MULTIPLIER_STORAGE"] = {"轴增量速度倍率存储", "游戏玩法"};
    m["AXIS_INCREMENTAL_SPEED_MULTIPLIER_DEFAULT"] = {"轴增量速度默认倍率", "游戏玩法"};
    m["AXIS_INCREMENTAL_SPEED_MULTIPLIER_VALUES"] = {"轴增量速度倍率值", "游戏玩法"};
    m["dontShowLauncher"] = {"不显示启动器", "游戏玩法"};

    // ===== 编辑器(VAB/SPH) =====
    m["VAB_USE_CLICK_PLACE"] = {"点击放置", "编辑器"};
    m["VAB_USE_ANGLE_SNAP"] = {"角度对齐", "编辑器"};
    m["VAB_ANGLE_SNAP_INCLUDE_VERTICAL"] = {"角度对齐含垂直", "编辑器"};
    m["VAB_FINE_OFFSET_THRESHOLD"] = {"精细偏移阈值", "编辑器"};
    m["VAB_CAMERA_ORBIT_SENS"] = {"VAB相机旋转灵敏度", "编辑器"};
    m["VAB_CAMERA_ZOOM_SENS"] = {"VAB相机缩放灵敏度", "编辑器"};
    m["VAB_CRAFTNAME_CHAR_LIMIT"] = {"飞船名称字符限制", "编辑器"};
    m["EDITOR_UNDO_REDO_LIMIT"] = {"编辑器撤销/重做限制", "编辑器"};
    m["SPACENAV_CAMERA_SENS_ROT"] = {"SpaceNav相机旋转灵敏度", "编辑器"};
    m["SPACENAV_CAMERA_SENS_LIN"] = {"SpaceNav相机线性灵敏度", "编辑器"};
    m["SPACENAV_CAMERA_SHARPNESS_LIN"] = {"SpaceNav相机线性锐度", "编辑器"};
    m["SPACENAV_CAMERA_SHARPNESS_ROT"] = {"SpaceNav相机旋转锐度", "编辑器"};
    m["SPACENAV_FLIGHT_SENS_ROT"] = {"SpaceNav飞行旋转灵敏度", "编辑器"};
    m["SPACENAV_FLIGHT_SENS_LIN"] = {"SpaceNav飞行线性灵敏度", "编辑器"};
    m["CONTROLPOINT_VISUALS_ENABLED"] = {"控制点可视化", "编辑器"};
    m["CONTROLPOINT_ARROWLENGTH"] = {"控制点箭头长度", "编辑器"};
    m["CONTROLPOINT_COLOR_FORWARD"] = {"控制点前方颜色", "编辑器"};
    m["CONTROLPOINT_COLOR_UP"] = {"控制点上方颜色", "编辑器"};
    m["CONTROLPOINT_COLOR_RIGHT"] = {"控制点右方颜色", "编辑器"};
    m["STAGE_GROUP_INFO_ITEMS"] = {"分级信息项", "编辑器"};
    m["STAGE_GROUP_INFO_WIDTH_EDITOR"] = {"编辑器分级信息宽度", "编辑器"};
    m["STAGE_GROUP_INFO_WIDTH_FLIGHT"] = {"飞行分级信息宽度", "编辑器"};
    m["STAGE_GROUP_INFO_NAME_PERCENTAGE"] = {"分级名称占比", "编辑器"};
    m["CRAFT_STEAM_UNSUBSCRIBE_WARNING"] = {"Steam取消订阅警告", "编辑器"};

    // ===== 相机 =====
    m["FLT_CAMERA_ORBIT_SENS"] = {"飞行相机旋转灵敏度", "相机"};
    m["FLT_CAMERA_ZOOM_SENS"] = {"飞行相机缩放灵敏度", "相机"};
    m["FLT_CAMERA_WOBBLE"] = {"飞行相机抖动", "相机"};
    m["FLT_CAMERA_CHASE_SHARPNESS"] = {"追踪相机锐度", "相机"};
    m["FLT_CAMERA_CHASE_USEVELOCITYVECTOR"] = {"追踪相机使用速度向量", "相机"};
    m["FLT_VESSEL_LABELS"] = {"飞船标签", "相机"};
    m["CAMERA_DOUBLECLICK_MOUSELOOK"] = {"双击鼠标视角", "相机"};
    m["DOUBLECLICK_MOUSESPEED"] = {"双击鼠标速度", "相机"};
    m["IVA_RETAIN_CONTROL_POINT"] = {"IVA保持控制点", "相机"};
    m["CAMERA_FX_EXTERNAL"] = {"外部相机特效", "相机"};
    m["CAMERA_FX_INTERNAL"] = {"内部相机特效", "相机"};

    // ===== EVA/舱外活动 =====
    m["EVA_ROTATE_ON_MOVE"] = {"移动时旋转", "EVA"};
    m["EVA_SHOW_PORTRAIT"] = {"显示肖像", "EVA"};
    m["EVA_DEFAULT_HELMET_ON"] = {"默认头盔开启", "EVA"};
    m["EVA_DEFAULT_NECKRING_ON"] = {"默认颈环开启", "EVA"};
    m["EVA_DIES_WHEN_UNSAFE_HELMET"] = {"不安全时摘头盔死亡", "EVA"};
    m["EVA_INHERIT_PART_TEMPERATURE"] = {"继承部件温度", "EVA"};
    m["EVA_SCREEN_MESSAGE_X"] = {"EVA消息X坐标", "EVA"};
    m["EVA_SCREEN_MESSAGE_Y"] = {"EVA消息Y坐标", "EVA"};
    m["EVA_LADDER_CHECK_END"] = {"梯子末端检查", "EVA"};
    m["EVA_LADDER_JOINT_WHEN_IDLE"] = {"闲置时梯子关节", "EVA"};
    m["EVA_LADDER_JOINT_BREAK_VELOCITY"] = {"梯子关节断裂速度", "EVA"};
    m["EVA_LADDER_JOINT_BREAK_ACCELERATION"] = {"梯子关节断裂加速度", "EVA"};
    m["EVA_MAX_SLOPE_ANGLE"] = {"最大坡度角", "EVA"};
    m["EVA_INVENTORY_RANGE"] = {"库存范围", "EVA"};
    m["EVA_CONSTRUCTION_RANGE"] = {"建造范围", "EVA"};
    m["EVA_CONSTRUCTION_COMBINE_ENABLED"] = {"启用合并建造", "EVA"};
    m["EVA_CONSTRUCTION_COMBINE_NONENGINEERS"] = {"非工程师合并建造", "EVA"};
    m["EVA_CONSTRUCTION_COMBINE_RANGE"] = {"合并建造范围", "EVA"};
    m["PART_REPAIR_MASS_PER_KIT"] = {"每套件修复质量", "EVA"};
    m["PART_REPAIR_MAX_KIT_AMOUNT"] = {"最大套件数量", "EVA"};

    // ===== 轨道 =====
    m["CONIC_PATCH_DRAW_MODE"] = {"轨道绘制模式", "轨道"};
    m["CONIC_PATCH_LIMIT"] = {"轨道显示数量", "轨道"};
    m["ALWAYS_SHOW_TARGET_APPROACH_MARKERS"] = {"始终显示目标接近标记", "轨道"};
    m["ORBIT_FADE_STRENGTH"] = {"轨道淡出强度", "轨道"};
    m["ORBIT_FADE_DIRECTION_INV"] = {"轨道淡出方向反转", "轨道"};
    m["ORBIT_WARP_DOWN_AT_SOI"] = {"SOI处时间加速降速", "轨道"};
    m["ORBIT_DRIFT_COMPENSATION"] = {"轨道漂移补偿", "轨道"};
    m["LEGACY_ORBIT_TARGETING"] = {"旧版轨道瞄准", "轨道"};
    m["SHOW_PWARP_WARNING"] = {"物理加速警告", "轨道"};
    m["ORBIT_WARP_MAXRATE_MODE"] = {"最大加速模式", "轨道"};
    m["ORBIT_WARP_PEMODE_SURFACE_MARGIN"] = {"近地点表面余量", "轨道"};
    m["ORBIT_WARP_ALTMODE_LIMIT_MODIFIER"] = {"高度模式限制修饰", "轨道"};
    m["RADAR_ALTIMETER_EXTENDED_CALCS"] = {"雷达高度计扩展计算", "轨道"};
    m["MAP_MAX_ORBIT_BEFORE_FORCE2D"] = {"强制2D前最大轨道数", "轨道"};

    // ===== 界面(UI) =====
    m["UI_SCALE"] = {"界面缩放", "界面"};
    m["UI_OPACITY"] = {"界面不透明度", "界面"};
    m["UI_MAINCANVAS_PIXEL_PERFECT"] = {"主画布像素对齐", "界面"};
    m["UI_ACTIONCANVAS_PIXEL_PERFECT"] = {"动作画布像素对齐", "界面"};
    m["UI_TOOLTIPCANVAS_PIXEL_PERFECT"] = {"提示画布像素对齐", "界面"};
    m["AUTOHIDE_NAVBALL"] = {"自动隐藏导航球", "界面"};
    m["WARP_TO_MANNODE_MARGIN"] = {"加速到机动节点余量", "界面"};
    m["UIELEMENTSCALINGENABLED"] = {"界面元素缩放启用", "界面"};
    m["UI_SCALE_TIME"] = {"时间缩放", "界面"};
    m["UI_SCALE_ALTIMETER"] = {"高度计缩放", "界面"};
    m["UI_SCALE_MAPOPTIONS"] = {"地图选项缩放", "界面"};
    m["UI_SCALE_APPS"] = {"应用缩放", "界面"};
    m["UI_SCALE_STAGINGSTACK"] = {"分级堆栈缩放", "界面"};
    m["UI_SCALE_MODE"] = {"模式缩放", "界面"};
    m["UI_SCALE_NAVBALL"] = {"导航球缩放", "界面"};
    m["UI_SCALE_CREW"] = {"船员缩放", "界面"};
    m["UI_POS_NAVBALL"] = {"导航球位置", "界面"};
    m["UI_COLOR_INACTIVE_TEXT"] = {"非激活文本颜色", "界面"};
    m["UI_COLOR_ACTIVE_TEXT"] = {"激活文本颜色", "界面"};
    m["UI_COLOR_INACTIVE_MINISETTNIGS_TEXT"] = {"非激活迷你设置文本颜色", "界面"};
    m["UI_POS_ALTIMETER_SLIDEDOWN_HOVER_HEIGHT"] = {"高度计悬停下滑高度", "界面"};
    m["MAPNODE_BEHINDBODY_OPACITY"] = {"天体后方节点不透明度", "界面"};
    m["COMMNET_LOWCOLOR_BRIGHTNESSFACTOR"] = {"通讯网络低亮度系数", "界面"};
    m["SHOW_VESSEL_NAMING_IN_FLIGHT"] = {"飞行中显示飞船命名", "界面"};
    m["VESSEL_NAMING_PRIORTY_LEVEL_MAX"] = {"飞船命名最大优先级", "界面"};
    m["VESSEL_NAMING_PRIORTY_LEVEL_DEFAULT"] = {"飞船命名默认优先级", "界面"};
    m["SCIENCE_EXPERIMENT_SHOW_TRANSFER_WARNING"] = {"科学实验传输警告", "界面"};
    m["NAVIGATION_GHOSTING"] = {"导航虚影", "界面"};
    m["VESSEL_ANCHOR_VELOCITY_THRESHOLD"] = {"飞船锚定速度阈值", "界面"};
    m["VESSEL_ANCHOR_TIME_THRESHOLD"] = {"飞船锚定时间阈值", "界面"};
    m["VESSEL_ANCHOR_ANGLE_CHANGE_THRESHOLD"] = {"飞船锚定角度变化阈值", "界面"};
    m["VESSEL_ANCHOR_ANGLE_TIME_THRESHOLD"] = {"飞船锚定角度时间阈值", "界面"};
    m["VESSEL_ANCHOR_BREAK_FORCE_FACTOR"] = {"飞船锚定断裂力系数", "界面"};
    m["VESSEL_ANCHOR_BREAK_TORQUE"] = {"飞船锚定断裂扭矩", "界面"};
    m["PAW_COLLAPSED_GROUP_NAMES"] = {"PAW折叠组名", "界面"};
    m["PAW_NUMERIC_SLIDERS"] = {"PAW数字滑块", "界面"};
    m["PAW_PREFERRED_HEIGHT"] = {"PAW首选高度", "界面"};
    m["PAW_SCREEN_OFFSET_X"] = {"PAW屏幕X偏移", "界面"};
    m["SHOW_DELETE_ALARM_CONFIRMATION"] = {"删除闹钟确认", "界面"};
    m["ALARM_ROW_DISPLAYED_FLIGHT"] = {"飞行中显示闹钟行", "界面"};
    m["SHOW_TRANSLATION_KEYS_ON_SCREEN"] = {"屏幕显示翻译键", "界面"};

    // ===== Delta-V =====
    m["DELTAV_CALCULATIONS_ENABLED"] = {"Delta-V计算启用", "Delta-V"};
    m["DELTAV_BURN_PERCENTAGE"] = {"燃烧百分比", "Delta-V"};
    m["DELTAV_BURN_ESTIMATE_COLORS"] = {"燃烧估算颜色", "Delta-V"};
    m["DELTAV_BURN_TIME_COLORS"] = {"燃烧时间颜色", "Delta-V"};
    m["DELTAV_ACTIVE_STAGE_UPDATE_SECS"] = {"激活级更新间隔", "Delta-V"};
    m["DELTAV_ALL_STAGES_UPDATE_SECS"] = {"所有级更新间隔", "Delta-V"};
    m["DELTAV_VESSEL_EVENT_DELAY_SECS"] = {"飞船事件延迟", "Delta-V"};
    m["DELTAV_ACTIVE_VESSEL_TIMESTEP"] = {"激活飞船时间步", "Delta-V"};
    m["DELTAV_CALCULATIONS_TIMESTEP"] = {"计算时间步", "Delta-V"};
    m["DELTAV_CALCULATIONS_BIGTIMESTEP"] = {"计算大时间步", "Delta-V"};
    m["DELTAV_USE_TIMED_VESSELCALCS"] = {"使用定时飞船计算", "Delta-V"};
    m["DELTAV_APP_ENABLED"] = {"Delta-V应用启用", "Delta-V"};
    m["DELTAV_APP_TWOCOLUMN_MODE"] = {"双列模式", "Delta-V"};
    m["LOG_DELTAV_VERBOSE"] = {"详细Delta-V日志", "Delta-V"};

    // ===== 车轮 =====
    m["WHEEL_WEIGHT_STRESS_MULTIPLIER"] = {"车轮重量压力倍率", "车轮"};
    m["WHEEL_SLIP_STRESS_MULTIPLIER"] = {"车轮滑动压力倍率", "车轮"};
    m["WHEEL_SUBSTEPS_ACTIVE"] = {"车轮激活子步数", "车轮"};
    m["WHEEL_SUBSTEPS_INACTIVE"] = {"车轮非激活子步数", "车轮"};
    m["WHEEL_AUTO_SPRINGDAMPER"] = {"自动弹簧阻尼", "车轮"};
    m["WHEEL_AUTO_STEERINGADJUST"] = {"自动转向调整", "车轮"};
    m["LEGS_ADVANCED_SUSPENSIONDAMPER"] = {"着陆腿高级悬挂阻尼", "车轮"};
    m["WHEEL_DAMAGE_IMPACTCOLLIDER_ENABLED"] = {"碰撞冲击损坏", "车轮"};
    m["WHEEL_DAMAGE_WHEELCOLLIDER_ENABLED"] = {"车轮碰撞损坏", "车轮"};

    // ===== KSPNet/通讯 =====
    m["KERBNET_ALIGNS_WITH_ORBIT"] = {"KerbNet与轨道对齐", "KerbNet"};
    m["KERBNET_REFRESH_FAST_INTERVAL"] = {"KerbNet快速刷新间隔", "KerbNet"};
    m["KERBNET_REFRESH_SLOW_INTERVAL"] = {"KerbNet慢速刷新间隔", "KerbNet"};
    m["KERBNET_BACKGROUND_FLUFF"] = {"KerbNet后台填充", "KerbNet"};

    // ===== 任务编辑器 =====
    m["MISSION_SHOW_CREATE_VESSEL_WARNING"] = {"创建飞船警告", "任务编辑器"};
    m["MISSION_SHOW_TEST_MISSION_WARNING"] = {"测试任务警告", "任务编辑器"};
    m["MISSION_SHOW_NO_BRIEFING_WARNING"] = {"无简报警告", "任务编辑器"};
    m["MISSION_STEAM_UNSUBSCRIBE_WARNING"] = {"任务Steam取消订阅警告", "任务编辑器"};
    m["MISSION_SNAP_TO_GRID"] = {"对齐网格", "任务编辑器"};
    m["MISSION_BUILDER_GAPHEIGHT"] = {"构建器间隙高度", "任务编辑器"};
    m["MISSION_SHOW_STOCK_PACKS_IN_BRIEFING"] = {"简报显示原版包", "任务编辑器"};
    m["MISSION_LOG_NODE_ACTIVATIONS"] = {"记录节点激活", "任务编辑器"};
    m["MISSION_SHOW_EXPANSION_INFO"] = {"显示扩展信息", "任务编辑器"};
    m["MISSION_MINIMUM_CANVAS_ZOOM"] = {"最小画布缩放", "任务编辑器"};
    m["MISSION_GAP_CAMERA_VAB_CONTROLS"] = {"间隙相机VAB控制", "任务编辑器"};
    m["MISSION_DELETE_REMOVES_IN_PROGRESS_MISSIONS"] = {"删除移除进行中任务", "任务编辑器"};
    m["MISSION_NAVIGATION_GHOSTING"] = {"任务导航虚影", "任务编辑器"};
    m["MISSION_VALIDATOR_MODE"] = {"任务验证模式", "任务编辑器"};
    m["MISSION_TEST_AUTOMATIC_CHECKPOINTS"] = {"自动检查点测试", "任务编辑器"};
    m["MANEUVER_TOOL_TRANSFER_DEGREES"] = {"机动工具转移角度", "任务编辑器"};
    m["MANEUVER_TOOL_CALC_TIMEOUT"] = {"机动工具计算超时", "任务编辑器"};
    m["MANEUVER_TOOL_CB_COLLISION_ADJUSTMENT"] = {"天体碰撞调整", "任务编辑器"};

    // ===== Serenity/Breaking Ground =====
    m["SERENITY_SHOW_EXPANSION_INFO"] = {"Serenity扩展信息", "Serenity"};
    m["SERENITY_ROCS_VISUAL_SPEED"] = {"ROC视觉速度", "Serenity"};
    m["SERENITY_CONTROLLER_IGNORES_VESSEL"] = {"控制器忽略飞船", "Serenity"};

    // ===== 教程 =====
    m["TUTORIALS_EDITOR_ENABLE"] = {"编辑器教程启用", "教程"};
    m["TUTORIALS_FLIGHT_ENABLE"] = {"飞行教程启用", "教程"};
    m["TUTORIALS_MISSION_SCREEN_TUTORIAL_COMPLETED"] = {"任务界面教程已完成", "教程"};
    m["TUTORIALS_MISSION_BUILDER_ENTERED"] = {"进入任务构建器", "教程"};
    m["TUTORIALS_ESA_MISSION_SCREEN_TUTORIAL_COMPLETED"] = {"ESA任务教程已完成", "教程"};

    // ===== 调试与日志 =====
    m["LOG_INSTANT_FLUSH"] = {"日志立即刷新", "调试与日志"};
    m["LOG_ERRORS_TO_SCREEN"] = {"错误显示到屏幕", "调试与日志"};
    m["LOG_EXCEPTIONS_TO_SCREEN"] = {"异常显示到屏幕", "调试与日志"};
    m["LOG_JOINT_BREAK_EVENT"] = {"记录关节断裂事件", "调试与日志"};
    m["LOG_MISSING_KEYS_TO_FILE"] = {"缺失键写入文件", "调试与日志"};
    m["LOG_FXMONGER_VERBOSE"] = {"详细FXMonger日志", "调试与日志"};
    m["COLLECT_ROC_STATS"] = {"收集ROC统计", "调试与日志"};
    m["DEBUG_MAX_SETPOSITION_ALTITUDE"] = {"最大SetPosition高度", "调试与日志"};
    m["DEBUG_AERO_GUI"] = {"气动调试界面", "调试与日志"};
    m["DEBUG_AERO_DATA_PAWS"] = {"气动数据PAW", "调试与日志"};
    m["FI_LOG_TEMP_ERROR"] = {"FI临时错误日志", "调试与日志"};
    m["FI_LOG_OVERTEMP"] = {"FI过热日志", "调试与日志"};

    // ===== 颜色设置 =====
    m["COLOR_PART_HIGHLIGHT"] = {"部件高亮颜色", "颜色设置"};
    m["COLOR_PART_EDITORATTACHED"] = {"编辑器已附着颜色", "颜色设置"};
    m["COLOR_PART_EDITORDETACHED"] = {"编辑器已分离颜色", "颜色设置"};
    m["COLOR_PART_ACTIONGROUP_SELECTED"] = {"动作组选中颜色", "颜色设置"};
    m["COLOR_PART_ACTIONGROUP_HIGHLIGHT"] = {"动作组高亮颜色", "颜色设置"};
    m["COLOR_PART_ROOTTOOL_HIGHLIGHT"] = {"根工具高亮颜色", "颜色设置"};
    m["COLOR_PART_ROOTTOOL_HIGHLIGHTEDGE"] = {"根工具高亮边缘颜色", "颜色设置"};
    m["COLOR_PART_ROOTTOOL_HOVER"] = {"根工具悬停颜色", "颜色设置"};
    m["COLOR_PART_ROOTTOOL_HOVEREDGE"] = {"根工具悬停边缘颜色", "颜色设置"};
    m["COLOR_PART_ENGINEERAPP_HIGHLIGHT"] = {"工程师应用高亮颜色", "颜色设置"};
    m["COLOR_PART_TRANSFER_SOURCE_HIGHLIGHT"] = {"传输源高亮颜色", "颜色设置"};
    m["COLOR_PART_TRANSFER_SOURCE_HOVER"] = {"传输源悬停颜色", "颜色设置"};
    m["COLOR_PART_TRANSFER_DEST_HIGHLIGHT"] = {"传输目标高亮颜色", "颜色设置"};
    m["COLOR_PART_TRANSFER_DEST_HOVER"] = {"传输目标悬停颜色", "颜色设置"};
    m["COLOR_PART_INVENTORY_CONTAINER"] = {"库存容器颜色", "颜色设置"};
    m["COLOR_PART_INVENTORY_NOSPACE"] = {"库存无空间颜色", "颜色设置"};
    m["COLOR_PART_CONSTRUCTION_VALID"] = {"建造有效颜色", "颜色设置"};
    m["COLOR_RD_SEARCH_NODE_HIGHLIGHT"] = {"RD搜索节点高亮", "颜色设置"};
    m["COLOR_RD_SEARCH_PART_HIGHLIGHT"] = {"RD搜索部件高亮", "颜色设置"};
    m["COLOR_LIGHT_PRESET_1"] = {"灯光预设1", "颜色设置"};
    m["COLOR_LIGHT_PRESET_2"] = {"灯光预设2", "颜色设置"};
    m["COLOR_LIGHT_PRESET_3"] = {"灯光预设3", "颜色设置"};
    m["COLOR_LIGHT_PRESET_4"] = {"灯光预设4", "颜色设置"};
    m["COLOR_LIGHT_PRESET_5"] = {"灯光预设5", "颜色设置"};
    m["COLOR_FIREWORK_PRESET_GREEN"] = {"烟花预设-绿", "颜色设置"};
    m["COLOR_FIREWORK_PRESET_LIGHT_BLUE"] = {"烟花预设-浅蓝", "颜色设置"};
    m["COLOR_FIREWORK_PRESET_BLUE"] = {"烟花预设-蓝", "颜色设置"};
    m["COLOR_FIREWORK_PRESET_PURPLE"] = {"烟花预设-紫", "颜色设置"};
    m["COLOR_FIREWORK_PRESET_PINK"] = {"烟花预设-粉", "颜色设置"};

    // ===== 其他 =====
    m["FEMALE_EYE_OFFSET_X"] = {"女性眼睛X偏移", "其他"};
    m["FEMALE_EYE_OFFSET_Y"] = {"女性眼睛Y偏移", "其他"};
    m["FEMALE_EYE_OFFSET_Z"] = {"女性眼睛Z偏移", "其他"};
    m["FEMALE_EYE_OFFSET_SCALE"] = {"女性眼睛偏移缩放", "其他"};

    return m;
}

KeyInfo getKeyInfo(const QString& key) {
    static QMap<QString, KeyInfo> info = createKeyInfoMap();
    auto it = info.find(key);
    if (it != info.end()) return it.value();
    return {key, "其他"};
}
}

InstanceManager& InstanceManager::instance()
{
    static InstanceManager inst;
    return inst;
}

InstanceManager::InstanceManager(QObject *parent)
    : QObject(parent), m_gameProcess(nullptr)
{
}

InstanceManager::~InstanceManager()
{
    if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) {
        m_gameProcess->kill();
        m_gameProcess->waitForFinished(3000);
    }
    delete m_gameProcess;
}

QList<GameSetting> InstanceManager::loadGameSettings(const QString &gamePath) const
{
    QList<GameSetting> settings;
    QString settingsPath = QDir(gamePath).filePath("settings.cfg");
    QFile file(settingsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return settings;
    }

    QTextStream in(&file);
    int braceDepth = 0;
    bool inBlock = false;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("//")) {
            continue;
        }

        if (line.contains('{')) {
            braceDepth++;
            if (braceDepth == 1 && !inBlock) {
                inBlock = true;
            }
            continue;
        }
        if (line.contains('}')) {
            braceDepth--;
            if (braceDepth == 0) {
                inBlock = false;
            }
            continue;
        }

        if (braceDepth == 0 && line.contains('=')) {
            int eqPos = line.indexOf('=');
            QString key = line.left(eqPos).trimmed();
            // Skip version entry
            if (key == "SETTINGS_FILE_VERSION") {
                continue;
            }
            GameSetting s;
            s.key = key;
            s.value = line.mid(eqPos + 1).trimmed();
            KeyInfo ki = getKeyInfo(key);
            s.displayName = ki.displayName;
            s.category = ki.category;
            settings.append(s);
        }
    }

    file.close();
    return settings;
}

bool InstanceManager::saveGameSettings(const QString &gamePath, const QList<GameSetting> &settings) const
{
    QString settingsPath = QDir(gamePath).filePath("settings.cfg");
    QFile file(settingsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    // Read original lines to preserve comments and structure
    QStringList originalLines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        originalLines.append(in.readLine());
    }
    file.close();

    // Build key-value map from new settings
    QMap<QString, QString> valueMap;
    for (const GameSetting& s : settings) {
        valueMap[s.key] = s.value;
    }

    // Rewrite lines updating values
    int braceDepth = 0;
    bool inBlock = false;
    for (int i = 0; i < originalLines.size(); ++i) {
        QString line = originalLines[i];
        QString trimmed = line.trimmed();

        if (trimmed.contains('{')) {
            braceDepth++;
            if (braceDepth == 1 && !inBlock) {
                inBlock = true;
            }
            continue;
        }
        if (trimmed.contains('}')) {
            braceDepth--;
            if (braceDepth == 0) {
                inBlock = false;
            }
            continue;
        }

        if (braceDepth == 0 && trimmed.contains('=') && !trimmed.startsWith("//")) {
            int eqPos = trimmed.indexOf('=');
            QString key = trimmed.left(eqPos).trimmed();
            if (valueMap.contains(key)) {
                int originalEqPos = line.indexOf('=');
                QString prefix = line.left(originalEqPos + 1);
                originalLines[i] = prefix + " " + valueMap[key];
            }
        }
    }

    // Write back
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream out(&file);
    for (const QString& l : originalLines) {
        out << l << "\n";
    }
    file.close();
    return true;
}

QList<DLCDetection> InstanceManager::detectDLCs(const QString &gamePath) const
{
    QList<DLCDetection> dlcs;

    DLCDetection mh;
    mh.id = "MakingHistory";
    mh.displayName = "Making History";
    mh.installed = QDir(QDir(gamePath).filePath("GameData/SquadExpansion/MakingHistory")).exists();
    dlcs.append(mh);

    DLCDetection bg;
    bg.id = "Serenity";
    bg.displayName = "Breaking Ground";
    bg.installed = QDir(QDir(gamePath).filePath("GameData/SquadExpansion/Serenity")).exists();
    dlcs.append(bg);

    return dlcs;
}

QStringList InstanceManager::listMods(const QString &gamePath) const
{
    QStringList mods;
    QString gameDataPath = QDir(gamePath).filePath("GameData");
    QDir gameData(gameDataPath);
    if (!gameData.exists()) {
        return mods;
    }

    QStringList entries = gameData.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList excluded = {"Squad", "SquadExpansion"};
    for (const QString& entry : entries) {
        if (!excluded.contains(entry, Qt::CaseInsensitive)) {
            mods.append(entry);
        }
    }

    return mods;
}

bool InstanceManager::launchGame(const QString &exePath)
{
    if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) {
        return false;
    }

    if (!m_gameProcess) {
        m_gameProcess = new QProcess(this);
        connect(m_gameProcess, &QProcess::started, this, &InstanceManager::gameStarted);
        connect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &InstanceManager::gameFinished);
        connect(m_gameProcess, &QProcess::errorOccurred, this, &InstanceManager::gameError);
    }

    QFileInfo exeInfo(exePath);
    m_gameProcess->setWorkingDirectory(exeInfo.absolutePath());
    m_gameProcess->start(exePath, QStringList());
    return true;
}

QString InstanceManager::detectGameRoot(const QString &exePath) const
{
    QFileInfo fi(exePath);
    return fi.absolutePath();
}

bool InstanceManager::isValidKSPPath(const QString &path) const
{
    QDir dir(path);
    return dir.exists("settings.cfg") && dir.exists("GameData");
}
