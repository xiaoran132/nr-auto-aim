import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, TimerAction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    params_file = os.path.join(
        get_package_share_directory("rm_vision_cpp"), "config", "params.yaml"
    )

    video_source = Node(
        package="rm_vision_cpp",
        executable="video_source",
        name="video_source",
        output="screen",
        parameters=[params_file, {"source": LaunchConfiguration("source")}],
    )

    armor_detector = Node(
        package="rm_vision_cpp",
        executable="armor_detector",
        name="armor_detector",
        output="screen",
        parameters=[params_file],
    )

    armor_tracker = Node(
        package="rm_vision_cpp",
        executable="armor_tracker",
        name="armor_tracker",
        output="screen",
        parameters=[params_file],
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "source", default_value="0",
                description="摄像头序号 / 视频路径 / 图片目录",
            ),
            video_source,
            TimerAction(period=1.0, actions=[armor_detector]),
            TimerAction(period=2.0, actions=[armor_tracker]),
        ]
    )
