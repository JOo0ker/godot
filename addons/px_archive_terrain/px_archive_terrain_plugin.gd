@tool
extends EditorPlugin

var _last_editor_camera: Camera3D

func _enter_tree() -> void:
	set_process(true)

func _exit_tree() -> void:
	set_process(false)
	_last_editor_camera = null

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
	if node.has_method("set_editor_camera_state") and node.has_method("is_using_camera_eye") and node.call("is_using_camera_eye"):
		node.call("set_editor_camera_state", camera_position, camera_frustum)

	for child in node.get_children():
		_sync_terrain_node(child, camera_position, camera_frustum)
