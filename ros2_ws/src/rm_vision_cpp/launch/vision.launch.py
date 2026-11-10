import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    params_file = os.path.join(
        get_package_share_directory("rm_vision_cpp"), "config", "params.yaml"
    )

    camera_config = DeclareLaunchArgument(
        "camera_config", default_value="", description="相机 YAML（空 = <repo>/configs/camera.yaml）"
    )
    calibration_file = DeclareLaunchArgument(
        "calibration_file", default_value="", description="标定 YAML（空 = <repo>/configs/calibration.yaml）"
    )
    model_xml = DeclareLaunchArgument(
        "model_xml", default_value="", description="模型 XML（空 = <repo>/assets/yolo11.xml）"
    )
    model_bin = DeclareLaunchArgument(
        "model_bin", default_value="", description="模型 BIN（空 = <repo>/assets/yolo11.bin）"
    )
    video_file = DeclareLaunchArgument(
        "video_file", default_value="", description="视频文件（非空则回放，无相机）"
    )
    enemy_color = DeclareLaunchArgument("enemy_color", default_value="red")

    camera_node = Node(
        package="rm_vision_cpp",
        executable="camera_node",
        name="camera_node",
        output="screen",
        emulate_tty=True,
        parameters=[
            params_file,
            {
                "camera_config": LaunchConfiguration("camera_config"),
                "calibration_file": LaunchConfiguration("calibration_file"),
                "video_file": LaunchConfiguration("video_file"),
            },
        ],
    )

    armor_detector = Node(
        package="rm_vision_cpp",
        executable="armor_detector",
        name="armor_detector",
        output="screen",
        emulate_tty=True,
        parameters=[
            params_file,
            {
                "model_xml": LaunchConfiguration("model_xml"),
                "model_bin": LaunchConfiguration("model_bin"),
                "enemy_color": LaunchConfiguration("enemy_color"),
            },
        ],
    )

    armor_tracker = Node(
        package="rm_vision_cpp",
        executable="armor_tracker",
        name="armor_tracker",
        output="screen",
        emulate_tty=True,
        parameters=[params_file],
    )

    return LaunchDescription(
        [
            camera_config,
            calibration_file,
            model_xml,
            model_bin,
            video_file,
            enemy_color,
            camera_node,
            armor_detector,
            armor_tracker,
        ]
    )
