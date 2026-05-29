@tool
extends EditorPlugin

var _last_editor_camera: Camera3D
var _editor_translations: Array = []

const _PROPERTY_TRANSLATIONS := {
	"Terrain Folder": "地形目录",
	"Origin Latitude": "原点纬度",
	"Origin Longitude": "原点经度",
	"Origin Altitude": "原点高度",
	"LOD": "地形等级",
	"Texture LOD": "贴图等级",
	"Max Tiles per Update": "每次最大瓦片数",
	"Max Tile Jobs per Refresh": "每次最大后台任务数",
	"Max Tile Attach per Frame": "每帧最大显示瓦片数",
	"Max Vertices per Tile": "单瓦片最大顶点数",
	"Archive Neighborhood": "归档邻域",
	"Vertical Range": "高度余量",
	"Loading Range": "加载范围",
	"Cleanup Range Multiplier": "清理范围倍数",
	"Max Tile Attach Time (ms)": "每帧显示时间上限毫秒",
	"Max Tile Attach Time Ms": "每帧显示时间上限毫秒",
	"Auto Refresh Interval": "自动刷新间隔",
	"Auto Refresh Min Distance": "自动刷新最小移动距离",
	"Auto Refresh Min Angle": "自动刷新最小旋转角度",
	"Worker Thread Count": "后台线程数",
}

const _DOCUMENTATION_TRANSLATIONS := {
	"Displays Phoenix terrain archive tiles around the active camera.": "围绕当前相机显示 Phoenix 地形归档瓦片。",
	"Loads terrain tiles from PX archive data near the current camera and attaches completed tiles to the scene.": "从当前相机附近的 PX 归档数据加载地形瓦片，并把加载完成的瓦片显示到场景中。",
	"Clears currently displayed terrain tiles.": "清空当前显示的地形瓦片。",
	"Requests a refresh for terrain tiles around the current camera.": "请求刷新当前相机周围的地形瓦片。",
	"Root folder that contains the archived terrain data.": "包含地形归档数据的根目录。",
	"Latitude represented by the scene origin.": "场景原点对应的纬度。",
	"Longitude represented by the scene origin.": "场景原点对应的经度。",
	"Altitude represented by the scene origin.": "场景原点对应的高度。",
	"Terrain detail level. Higher values show finer terrain and cost more work.": "地形细节等级。数值越高，地形越细，但工作量也越大。",
	"Texture detail level. Higher values show clearer textures and cost more work.": "贴图细节等级。数值越高，贴图越清晰，但工作量也越大。",
	"Compatibility value that also updates the maximum background tile jobs per refresh.": "兼容旧配置的数值，同时会同步每次刷新允许安排的后台瓦片任务数量。",
	"Maximum number of tile load jobs scheduled by one refresh.": "一次刷新最多安排多少个瓦片加载任务。",
	"Maximum number of loaded tiles attached to the scene in one frame.": "一帧内最多把多少个已加载瓦片显示到场景中。",
	"Maximum number of vertices read from one tile.": "单个瓦片最多读取多少个顶点。",
	"Number of extra archive rings opened around the camera archive.": "在相机所在归档周围额外打开几圈归档。",
	"Vertical margin used when deciding whether a tile is near the camera view.": "判断瓦片是否靠近相机视野时使用的上下高度余量。",
	"Distance around the camera where tiles are allowed to load.": "相机周围允许加载瓦片的距离范围。",
	"Multiplier that decides how far displayed tiles can be from the loading range before they are cleared.": "已显示瓦片超过加载范围多少倍后才会被清理。",
	"Maximum frame time spent attaching newly loaded tiles to the scene.": "每帧用于显示新瓦片的最长时间。",
	"Time between camera checks for loading new terrain.": "多久检查一次相机位置和方向，以决定是否加载新地形。",
	"Camera movement distance needed before a refresh is requested.": "相机移动超过这个距离后才请求刷新。",
	"Camera rotation angle needed before a refresh is requested.": "相机旋转超过这个角度后才请求刷新。",
	"Number of background threads used for tile loading.": "用于加载瓦片的后台线程数量。",
}

func _enter_tree() -> void:
	_register_editor_translations()
	set_process(true)

func _exit_tree() -> void:
	set_process(false)
	_last_editor_camera = null
	_unregister_editor_translations()

func _process(_delta: float) -> void:
	if _last_editor_camera:
		_sync_editor_camera(_last_editor_camera)

func _forward_3d_gui_input(camera: Camera3D, _event: InputEvent) -> int:
	_last_editor_camera = camera
	_sync_editor_camera(camera)
	return 0

func _sync_editor_camera(camera: Camera3D) -> void:
	var root := get_editor_interface().get_edited_scene_root()
	if root == null:
		return
	_sync_terrain_node(root, camera.global_position, camera.get_frustum())

func _sync_terrain_node(node: Node, camera_position: Vector3, camera_frustum: Array) -> void:
	if node.has_method("set_editor_camera_state"):
		node.call("set_editor_camera_state", camera_position, camera_frustum)

	for child in node.get_children():
		_sync_terrain_node(child, camera_position, camera_frustum)

func _register_editor_translations() -> void:
	_unregister_editor_translations()
	_add_translation_to_domain("godot.properties", _PROPERTY_TRANSLATIONS)
	_add_translation_to_domain("godot.documentation", _DOCUMENTATION_TRANSLATIONS)

func _unregister_editor_translations() -> void:
	if _editor_translations.is_empty():
		return

	var property_domain := TranslationServer.get_or_add_domain("godot.properties")
	var documentation_domain := TranslationServer.get_or_add_domain("godot.documentation")
	for translation in _editor_translations:
		property_domain.remove_translation(translation)
		documentation_domain.remove_translation(translation)
	_editor_translations.clear()

func _add_translation_to_domain(domain_name: StringName, messages: Dictionary) -> void:
	var domain := TranslationServer.get_or_add_domain(domain_name)
	for locale in ["zh_CN", "zh"]:
		var translation := Translation.new()
		translation.locale = locale
		for source in messages:
			translation.add_message(source, messages[source])
		domain.add_translation(translation)
		_editor_translations.append(translation)
