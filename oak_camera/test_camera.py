#!/usr/bin/env python3
"""Quick test: capture one RGB frame and one depth frame from Oak D Lite (DepthAI v3)."""

import time
import cv2
import numpy as np
import depthai as dai

print("Building pipeline...")
pipeline = dai.Pipeline()

# RGB camera (v3 uses Camera node)
cam = pipeline.create(dai.node.Camera)
cam.setBoardSocket(dai.CameraBoardSocket.CAM_A)

# Stereo depth
mono_left = pipeline.create(dai.node.MonoCamera)
mono_left.setResolution(dai.MonoCameraProperties.SensorResolution.THE_400_P)
mono_left.setBoardSocket(dai.CameraBoardSocket.CAM_B)

mono_right = pipeline.create(dai.node.MonoCamera)
mono_right.setResolution(dai.MonoCameraProperties.SensorResolution.THE_400_P)
mono_right.setBoardSocket(dai.CameraBoardSocket.CAM_C)

stereo = pipeline.create(dai.node.StereoDepth)
stereo.setDefaultProfilePreset(dai.node.StereoDepth.PresetMode.DEFAULT)
stereo.setLeftRightCheck(True)

mono_left.out.link(stereo.left)
mono_right.out.link(stereo.right)

# Create output queues from node outputs (v3 API)
q_depth = stereo.depth.createOutputQueue()

print("Connecting to Oak D Lite...")
pipeline.start()

with dai.Device(pipeline) as device:
    print(f"Connected: {device.getDeviceName()}, USB: {device.getUsbSpeed()}")

    # Request RGB output after device is ready
    q_rgb = cam.requestOutput((640, 400), dai.ImgFrame.Type.BGR888p).createOutputQueue()

    print("Waiting for frames...")
    rgb_frame = None
    depth_frame = None

    for _ in range(50):
        if rgb_frame is None:
            pkt = q_rgb.tryGet()
            if pkt:
                rgb_frame = pkt.getCvFrame()
        if depth_frame is None:
            pkt = q_depth.tryGet()
            if pkt:
                depth_frame = pkt.getFrame()
        if rgb_frame is not None and depth_frame is not None:
            break
        time.sleep(0.1)

    if rgb_frame is not None:
        cv2.imwrite("test_rgb.jpg", rgb_frame)
        print(f"RGB: saved test_rgb.jpg ({rgb_frame.shape})")
    else:
        print("ERROR: No RGB frame received")

    if depth_frame is not None:
        valid = depth_frame[depth_frame > 0]
        print(f"Depth: {depth_frame.shape}, "
              f"range {np.min(valid)/1000:.2f}m - {np.max(valid)/1000:.2f}m, "
              f"avg {np.mean(valid)/1000:.2f}m")
        depth_color = cv2.applyColorMap(
            cv2.normalize(depth_frame, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8),
            cv2.COLORMAP_JET)
        cv2.imwrite("test_depth.jpg", depth_color)
        print(f"Depth: saved test_depth.jpg")
    else:
        print("ERROR: No depth frame received")

print("\nCamera working!" if rgb_frame is not None else "\nCamera test FAILED.")
