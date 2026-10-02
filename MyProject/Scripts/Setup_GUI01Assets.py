# 踏みつけ加速メカゲーム — GUI-01 汎用 HUD 部品のアセット生成スクリプト
#
# 生成・更新するアセット:
#   /Game/UI/Textures/HUD/T_HUD_SpeedFrame / T_HUD_SpeedBack / T_HUD_SpeedFill
#                                         （C:\ImpactVehicleGame\Image の画像を取り込む。毎回取り込み直す）
#   /Game/UI/Materials/M_HUD_ArcFill      （塗り画像を角度で切り抜く弧マスク）
#   /Game/UI/Curves/CF_HUD_SpeedResponse  （速度比 → メーター上の位置。破壊閾値・超加速をセグメントの区切りに合わせる）
#   /Game/UI/WBP_HUDMeter / WBP_HUDImageSwitch / WBP_HUDText
#                                         （汎用 HUD 部品の見た目のテンプレート。WBP_HUDMeter には速度メーターの画像を既定で入れる）
#   /Game/UI/WBP_BattleHUD                （対戦 HUD。部品の配置はデザイナーで行う。既存なら中身に触れない）
#   /Game/Blueprints/BP_ImpactPlayerController （WBP_BattleHUD を割り当て）
#   /Game/UI/WBP_HUD                      （横棒の速度ゲージを隠す。速度は WBP_BattleHUD のメーターで見せる）
#
# DA_VehicleTuning / DA_ImpactTuning の破壊閾値・超加速の速度を変えたら、このスクリプトを再実行して
# CF_HUD_SpeedResponse を作り直すこと。
#
# 画像は UE プロジェクトの外（C:\ImpactVehicleGame\Image）にある。元のファイルは移動・削除しない。
# 画像を描き直した場合は、同じファイル名で置き換えてこのスクリプトを再実行する。
#
# 何度実行しても同じ結果になる（冪等）。
#
# 実行方法（エディタを閉じた状態で）:
#   UnrealEditor.exe <uproject> -ExecutePythonScript="<このファイル>" -unattended -nosplash -nop4

import os

import unreal

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

# UE プロジェクトの外にある画像の置き場所。このファイル（Project/MyProject/Scripts）から 3 階層上。
IMAGE_DIR = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "Image"))

TEXTURE_PATH = "/Game/UI/Textures/HUD"
MATERIAL_PATH = "/Game/UI/Materials"
ARC_MATERIAL_NAME = "M_HUD_ArcFill"
ARC_MATERIAL_PATH = "{0}/{1}".format(MATERIAL_PATH, ARC_MATERIAL_NAME)

# (アセット名, 画像ファイル名)
TEXTURES = [
    ("T_HUD_SpeedFrame", "_0002_Base.png"),
    ("T_HUD_SpeedBack", "_0001_Non_Grah.png"),
    ("T_HUD_SpeedFill", "_0000_clor_Grah.png"),
]

# マテリアルのパラメータ名。C++（TreadHUDMeterWidget.cpp の ArcParameter）と一致させること。
PARAM_TEXTURE = "FillTexture"
PARAM_FILL = "Fill"
PARAM_PIVOT = "PivotUV"
PARAM_START = "StartAngle"
PARAM_SWEEP = "SweepAngle"
PARAM_ASPECT = "Aspect"
SCALAR_PARAMETERS = [PARAM_FILL, PARAM_START, PARAM_SWEEP, PARAM_ASPECT]

# 弧マスク（GUI_SPEC 4.2）。UV は Y が下向きなので角度の計算で反転する。
# SweepAngle が 0 のときの 0 除算を避ける。Fill が 0 のときは何も表示しない。
ARC_HLSL = """float2 d = UV - PivotUV;
d.x *= Aspect;
float ang = degrees(atan2(-d.y, d.x));
float rel = (SweepAngle >= 0) ? (ang - StartAngle) : (StartAngle - ang);
rel = fmod(rel + 720.0, 360.0);
float t = rel / max(abs(SweepAngle), 0.001);
return (Fill > 0.0 && t <= Fill) ? 1.0 : 0.0;"""

# Custom ノードの入力名（HLSL の変数名）
ARC_INPUTS = ["UV", "PivotUV", "StartAngle", "SweepAngle", "Aspect", "Fill"]

UI_PATH = "/Game/UI"
CURVE_PATH = "/Game/UI/Curves"
SPEED_CURVE_NAME = "CF_HUD_SpeedResponse"
SPEED_CURVE_PATH = "{0}/{1}".format(CURVE_PATH, SPEED_CURVE_NAME)
VEHICLE_TUNING_PATH = "/Game/Tuning/DA_VehicleTuning"
IMPACT_TUNING_PATH = "/Game/Tuning/DA_ImpactTuning"
CONTROLLER_BP_PATH = "/Game/Blueprints/BP_ImpactPlayerController"
HUD_WIDGET_PATH = "/Game/UI/WBP_HUD"

# 速度メーターの画像（C:\ImpactVehicleGame\Image）から実測した値（GUI-01 Phase 4）。
# 画像を描き直した場合は測り直す（README_TreadHUD.md「弧の値の求め方」）。
#   塗り画像（206x276）上の円の中心 (160, 162.1) px
SPEED_PIVOT_UV = (160.0 / 206.0, 162.1 / 276.0)
#   左下（赤の端）が 0%。そこから時計回りに青の端まで。
SPEED_START_ANGLE = -138.5
SPEED_SWEEP_ANGLE = -151.5
#   赤・黄・緑・青の区切り（弧上の比率）
SPEED_SEGMENT_END_RATIOS = [0.2, 0.455, 0.726, 1.0]
#   塗り・未到達の画像を枠の画像に重ねる位置（px）。枠は (0, 0)。
SPEED_GAUGE_LAYER_OFFSET = (3.0, 7.0)
#   表示値の追従速度（大きいほど速く追いつく）。
SPEED_INTERP_SPEED = 12.0

# (アセット名, 親クラス)。WBP_BattleHUD は配置だけを担当するため素の UserWidget を親にする。
WIDGETS = [
    ("WBP_HUDMeter", unreal.TreadHUDMeterWidget),
    ("WBP_HUDImageSwitch", unreal.TreadHUDImageSwitchWidget),
    ("WBP_HUDText", unreal.TreadHUDTextWidget),
    ("WBP_BattleHUD", unreal.UserWidget),
]


def log(message):
    unreal.log("[GUI01Setup] {0}".format(message))


def fail(message):
    unreal.log_error("[GUI01Setup] FAILED: {0}".format(message))
    raise RuntimeError(message)


def load_required(path):
    asset = EAL.load_asset(path)
    if asset is None:
        fail("could not load required asset: {0}".format(path))
    return asset


def enum_value(enum_type, *candidates):
    """候補名を順に試す。全て外れた場合は実在する定数名をログに出して停止する。"""
    for name in candidates:
        value = getattr(enum_type, name, None)
        if value is not None:
            return value
    available = [a for a in dir(enum_type) if a.isupper()]
    fail("no candidate {0} on {1}; available: {2}".format(candidates, enum_type.__name__, available))


def set_property(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as error:
        fail("could not set {0} on {1}: {2}".format(name, obj.get_name(), error))


def ensure_directory(path):
    if not EAL.does_directory_exist(path):
        EAL.make_directory(path)
        log("created directory: {0}".format(path))


def save(asset, path):
    if not EAL.save_loaded_asset(asset, only_if_is_dirty=False):
        fail("could not save {0}".format(path))


# ---------------------------------------------------------------------------
# Phase 4: テクスチャ・弧マスク
# ---------------------------------------------------------------------------

def import_texture(asset_name, file_name):
    """画像を取り込み（既存なら置き換え）、UI 用の設定にする。"""
    source = os.path.join(IMAGE_DIR, file_name)
    if not os.path.isfile(source):
        fail("image not found: {0}".format(source))

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", TEXTURE_PATH)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    ASSET_TOOLS.import_asset_tasks([task])

    path = "{0}/{1}".format(TEXTURE_PATH, asset_name)
    texture = load_required(path)
    if not isinstance(texture, unreal.Texture2D):
        fail("{0} is not a Texture2D".format(path))

    # UI 用: 圧縮しない RGBA（UserInterface2D）、ミップマップなし、UI のテクスチャグループ。
    set_property(texture, "compression_settings", enum_value(unreal.TextureCompressionSettings, "TC_EDITOR_ICON"))
    set_property(texture, "mip_gen_settings", enum_value(unreal.TextureMipGenSettings, "TMGS_NO_MIPMAPS"))
    set_property(texture, "lod_group", enum_value(unreal.TextureGroup, "TEXTUREGROUP_UI"))
    save(texture, path)

    log("imported {0} <- {1} ({2}x{3})".format(path, file_name, texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
    return texture


def create_arc_material(fill_texture):
    """M_HUD_ArcFill を作る（既存ならノードを作り直す）。"""
    ensure_directory(MATERIAL_PATH)
    if EAL.does_asset_exist(ARC_MATERIAL_PATH):
        material = load_required(ARC_MATERIAL_PATH)
        MEL.delete_all_material_expressions(material)
        log("rebuilding: {0}".format(ARC_MATERIAL_PATH))
    else:
        material = ASSET_TOOLS.create_asset(ARC_MATERIAL_NAME, MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())
        if material is None:
            fail("could not create {0}".format(ARC_MATERIAL_PATH))
        log("created: {0}".format(ARC_MATERIAL_PATH))

    set_property(material, "material_domain", enum_value(unreal.MaterialDomain, "MD_UI"))
    set_property(material, "blend_mode", enum_value(unreal.BlendMode, "BLEND_MASKED"))

    texture = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, -200)
    set_property(texture, "parameter_name", PARAM_TEXTURE)
    set_property(texture, "texture", fill_texture)
    set_property(texture, "sampler_type", enum_value(unreal.MaterialSamplerType, "SAMPLERTYPE_COLOR"))

    uv = MEL.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, 100)

    pivot = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1100, 200)
    set_property(pivot, "parameter_name", PARAM_PIVOT)
    pivot_mask = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, -900, 200)
    set_property(pivot_mask, "r", True)
    set_property(pivot_mask, "g", True)
    set_property(pivot_mask, "b", False)
    set_property(pivot_mask, "a", False)
    MEL.connect_material_expressions(pivot, "", pivot_mask, "")

    scalars = {}
    for index, name in enumerate(SCALAR_PARAMETERS):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 350 + index * 120)
        set_property(node, "parameter_name", name)
        scalars[name] = node

    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -500, 200)
    set_property(custom, "code", ARC_HLSL)
    set_property(custom, "description", "ArcMask")
    set_property(custom, "output_type", enum_value(unreal.CustomMaterialOutputType, "CMOT_FLOAT1"))
    inputs = []
    for name in ARC_INPUTS:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    set_property(custom, "inputs", inputs)

    connections = [(uv, "UV"), (pivot_mask, "PivotUV")] + [(scalars[name], name) for name in SCALAR_PARAMETERS]
    for source, input_name in connections:
        if not MEL.connect_material_expressions(source, "", custom, input_name):
            fail("could not connect {0} to the arc mask".format(input_name))

    # 塗り画像のアルファに弧マスクを掛けたものを不透明度マスクにする。色は画像そのもの（マテリアルで色分けしない）。
    opacity = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -250, 100)
    if not MEL.connect_material_expressions(texture, "A", opacity, "A"):
        fail("could not connect the texture alpha")
    MEL.connect_material_expressions(custom, "", opacity, "B")

    if not MEL.connect_material_property(texture, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        fail("could not connect the final color")
    if not MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY_MASK):
        fail("could not connect the opacity mask")

    MEL.recompile_material(material)
    save(material, ARC_MATERIAL_PATH)
    return material


def verify_arc_material(material):
    scalar_names = [str(name) for name in MEL.get_scalar_parameter_names(material)]
    vector_names = [str(name) for name in MEL.get_vector_parameter_names(material)]
    texture_names = [str(name) for name in MEL.get_texture_parameter_names(material)]
    for name in SCALAR_PARAMETERS:
        if name not in scalar_names:
            fail("{0} lacks scalar parameter {1} (has {2})".format(ARC_MATERIAL_PATH, name, scalar_names))
    if PARAM_PIVOT not in vector_names:
        fail("{0} lacks vector parameter {1} (has {2})".format(ARC_MATERIAL_PATH, PARAM_PIVOT, vector_names))
    if PARAM_TEXTURE not in texture_names:
        fail("{0} lacks texture parameter {1} (has {2})".format(ARC_MATERIAL_PATH, PARAM_TEXTURE, texture_names))
    if material.get_editor_property("material_domain") != unreal.MaterialDomain.MD_UI:
        fail("{0} is not a UI material".format(ARC_MATERIAL_PATH))
    log("verified: {0} parameters {1} / {2} / {3}".format(ARC_MATERIAL_PATH, scalar_names, vector_names, texture_names))


# ---------------------------------------------------------------------------
# Phase 5: 速度の割り当てカーブ・メーターのテンプレート
# ---------------------------------------------------------------------------

def compute_speed_curve_keys():
    """速度比の区切り（破壊閾値 Small / Large・超加速）を、画像のセグメントの区切りへ写すキーを求める。"""
    vehicle = load_required(VEHICLE_TUNING_PATH)
    rules = load_required(IMPACT_TUNING_PATH)
    max_speed = vehicle.get_editor_property("max_speed")
    if max_speed <= 0.0:
        fail("DA_VehicleTuning.MaxSpeed must be positive (is {0})".format(max_speed))

    thresholds = [
        vehicle.get_editor_property("small_destruction_speed"),
        vehicle.get_editor_property("large_destruction_speed"),
        rules.get_editor_property("overdrive_speed"),
    ]
    ratios = [speed / max_speed for speed in thresholds]
    # 0 < Small < Large < 超加速 < MaxSpeed（比率で 0 < r0 < r1 < r2 < 1）
    if not all(a < b for a, b in zip([0.0] + ratios, ratios + [1.0])):
        fail("speed thresholds {0} must increase strictly below MaxSpeed {1}".format(thresholds, max_speed))

    # 赤 = Small 未満、黄 = Large 未満、緑 = 超加速未満、青 = 超加速。
    segment_starts = SPEED_SEGMENT_END_RATIOS[:len(ratios)]
    keys = [(0.0, 0.0)] + list(zip(ratios, segment_starts)) + [(1.0, 1.0)]
    log("speed curve keys (speed ratio -> meter): {0} from thresholds {1} / max {2}".format(
        ["({0:.3f}, {1:.3f})".format(x, y) for x, y in keys], thresholds, max_speed))
    return keys


def create_speed_curve():
    ensure_directory(CURVE_PATH)
    if EAL.does_asset_exist(SPEED_CURVE_PATH):
        curve = load_required(SPEED_CURVE_PATH)
    else:
        curve = ASSET_TOOLS.create_asset(SPEED_CURVE_NAME, CURVE_PATH, unreal.CurveFloat, unreal.CurveFloatFactory())
        if curve is None:
            fail("could not create {0}".format(SPEED_CURVE_PATH))
        log("created: {0}".format(SPEED_CURVE_PATH))

    keys = compute_speed_curve_keys()
    if not unreal.TreadHUDLibrary.set_linear_curve_keys(curve, [unreal.Vector2D(x, y) for x, y in keys]):
        fail("could not set keys on {0}".format(SPEED_CURVE_PATH))
    save(curve, SPEED_CURVE_PATH)

    # 保存後に評価して、超加速の速度比が青の始端に写ることを確かめる。
    overdrive_ratio, blue_start = keys[-2]
    evaluated = curve.get_float_value(overdrive_ratio)
    if abs(evaluated - blue_start) > 0.001:
        fail("{0}({1:.3f}) = {2:.3f}, expected {3:.3f}".format(SPEED_CURVE_NAME, overdrive_ratio, evaluated, blue_start))
    return curve


def create_widget_blueprint(name, parent_class):
    """WBP を作る。既存なら中身（デザイナーの配置）には触れず、コンパイルと保存だけ行う。"""
    full_path = "{0}/{1}".format(UI_PATH, name)
    if EAL.does_asset_exist(full_path):
        log("already exists: {0}".format(full_path))
        blueprint = load_required(full_path)
    else:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = ASSET_TOOLS.create_asset(name, UI_PATH, unreal.WidgetBlueprint, factory)
        if blueprint is None:
            fail("could not create {0}".format(full_path))
        log("created: {0}".format(full_path))

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint, full_path)

    if not isinstance(unreal.get_default_object(blueprint.generated_class()), parent_class):
        fail("{0} does not derive from {1}".format(name, parent_class.__name__))
    return blueprint


def configure_blueprint(blueprint, properties):
    """CDO にプロパティを設定し、コンパイルして保存する。"""
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    for name, value in properties:
        set_property(cdo, name, value)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint, blueprint.get_path_name())
    log("{0}: set {1}".format(blueprint.get_name(), ", ".join(name for name, _ in properties)))


def speed_meter_defaults(textures, material):
    gauge_offset = unreal.Vector2D(*SPEED_GAUGE_LAYER_OFFSET)
    return [
        ("frame_texture", textures["T_HUD_SpeedFrame"]),
        ("frame_layer_offset", unreal.Vector2D(0.0, 0.0)),
        ("background_texture", textures["T_HUD_SpeedBack"]),
        ("background_layer_offset", gauge_offset),
        ("fill_texture", textures["T_HUD_SpeedFill"]),
        ("fill_layer_offset", gauge_offset),
        ("fill_material", material),
        ("pivot_uv", unreal.Vector2D(*SPEED_PIVOT_UV)),
        ("start_angle", SPEED_START_ANGLE),
        ("sweep_angle", SPEED_SWEEP_ANGLE),
        ("segment_end_ratios", SPEED_SEGMENT_END_RATIOS),
        ("interp_speed", SPEED_INTERP_SPEED),
    ]


# ---------------------------------------------------------------------------
# Phase 6: 対戦 HUD の割り当て
# ---------------------------------------------------------------------------

def bool_property_name(obj, *candidates):
    """bool の UPROPERTY は Python で接頭辞 b が外れる。実在する名前を返す。"""
    for name in candidates:
        try:
            obj.get_editor_property(name)
            return name
        except Exception:
            continue
    fail("none of {0} exists on {1}".format(candidates, obj.get_name()))


def assign_battle_hud(battle_hud):
    controller = load_required(CONTROLLER_BP_PATH)
    configure_blueprint(controller, [("battle_hud_widget_class", battle_hud.generated_class())])

    hud = load_required(HUD_WIDGET_PATH)
    hud_cdo = unreal.get_default_object(hud.generated_class())
    configure_blueprint(hud, [(bool_property_name(hud_cdo, "show_speed_gauge", "b_show_speed_gauge"), False)])


def verify(textures, material, curve, widgets):
    def cdo_of(blueprint):
        return unreal.get_default_object(load_required(blueprint.get_path_name()).generated_class())

    meter = cdo_of(widgets["WBP_HUDMeter"])
    for name, expected in speed_meter_defaults(textures, material):
        actual = meter.get_editor_property(name)
        if isinstance(expected, unreal.Object):
            if actual is None or actual.get_path_name() != expected.get_path_name():
                fail("WBP_HUDMeter.{0} is {1}, expected {2}".format(name, actual, expected.get_path_name()))
        elif isinstance(expected, list):
            if [round(v, 4) for v in actual] != [round(v, 4) for v in expected]:
                fail("WBP_HUDMeter.{0} is {1}, expected {2}".format(name, list(actual), expected))
        elif isinstance(expected, unreal.Vector2D):
            if abs(actual.x - expected.x) > 1e-4 or abs(actual.y - expected.y) > 1e-4:
                fail("WBP_HUDMeter.{0} is {1}, expected {2}".format(name, actual, expected))
        elif abs(actual - expected) > 1e-4:
            fail("WBP_HUDMeter.{0} is {1}, expected {2}".format(name, actual, expected))

    controller = unreal.get_default_object(load_required(CONTROLLER_BP_PATH).generated_class())
    assigned = controller.get_editor_property("battle_hud_widget_class")
    if assigned is None or assigned.get_path_name() != widgets["WBP_BattleHUD"].generated_class().get_path_name():
        fail("BP_ImpactPlayerController.BattleHUDWidgetClass is {0}".format(assigned))
    for name in ("hud_widget_class", "result_widget_class"):
        if controller.get_editor_property(name) is None:
            fail("BP_ImpactPlayerController.{0} was cleared".format(name))

    hud = unreal.get_default_object(load_required(HUD_WIDGET_PATH).generated_class())
    if hud.get_editor_property(bool_property_name(hud, "show_speed_gauge", "b_show_speed_gauge")):
        fail("WBP_HUD still shows the speed gauge")

    if curve.get_float_value(1.0) < 0.999:
        fail("{0}(1) must be 1".format(SPEED_CURVE_NAME))

    log("verified: meter template, speed curve, battle HUD assignment and WBP_HUD gauge setting")


def main():
    log("=== GUI-01 HUD asset setup start (images: {0}) ===".format(IMAGE_DIR))
    ensure_directory(TEXTURE_PATH)
    textures = {name: import_texture(name, file_name) for name, file_name in TEXTURES}

    material = create_arc_material(textures["T_HUD_SpeedFill"])
    verify_arc_material(material)

    curve = create_speed_curve()

    widgets = {name: create_widget_blueprint(name, parent) for name, parent in WIDGETS}
    configure_blueprint(widgets["WBP_HUDMeter"], speed_meter_defaults(textures, material))

    assign_battle_hud(widgets["WBP_BattleHUD"])

    verify(textures, material, curve, widgets)
    log("=== GUI-01 HUD asset setup complete ===")


main()
unreal.SystemLibrary.quit_editor()
