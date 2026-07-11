"""生成 Carbon NPU ONNX 模型 - 简化版"""
import numpy as np
import onnx
from onnx import helper, TensorProto, numpy_helper
import os
import onnxruntime as ort

OUTPUT_DIR = r"E:\Carbon\carbon\models"
os.makedirs(OUTPUT_DIR, exist_ok=True)

def create_chunk_mesh_model():
    """区块网格面剔除"""
    block_data = helper.make_tensor_value_info("block_data", TensorProto.INT32, [1, 16, 256, 16])
    face_mask = helper.make_tensor_value_info("face_mask", TensorProto.FLOAT, [1, 6, 16, 256, 16])
    vertex_count = helper.make_tensor_value_info("vertex_count", TensorProto.INT32, [1])

    nodes = []
    inits = []

    nodes.append(helper.make_node("Cast", ["block_data"], ["blocks_f"], to=TensorProto.FLOAT))

    pads = numpy_helper.from_array(np.array([0,1,1,1, 0,1,1,1], dtype=np.int64), "pads_const")
    val = numpy_helper.from_array(np.array(0.0, dtype=np.float32), "pad_val")
    inits += [pads, val]
    nodes.append(helper.make_node("Pad", ["blocks_f", "pads_const", "pad_val"], ["padded"]))

    dirs = [
        ("n_px", [0,1,1,2], [1,17,257,18]),
        ("n_mx", [0,1,1,0], [1,17,257,16]),
        ("n_py", [0,2,1,1], [1,18,257,17]),
        ("n_my", [0,0,1,1], [1,16,257,17]),
        ("n_pz", [0,1,2,1], [1,17,258,17]),
        ("n_mz", [0,1,0,1], [1,17,256,17]),
    ]
    axes4 = numpy_helper.from_array(np.array([0,1,2,3], dtype=np.int64), "axes4")
    steps4 = numpy_helper.from_array(np.array([1,1,1,1], dtype=np.int64), "steps4")
    inits += [axes4, steps4]

    for dirname, s, e in dirs:
        si = numpy_helper.from_array(np.array(s, dtype=np.int64), f"s_{dirname}")
        ei = numpy_helper.from_array(np.array(e, dtype=np.int64), f"e_{dirname}")
        inits += [si, ei]
        nodes.append(helper.make_node("Slice", ["padded", f"s_{dirname}", f"e_{dirname}", "axes4", "steps4"], [dirname]))

    s_center = numpy_helper.from_array(np.array([0,1,1,1], dtype=np.int64), "s_center")
    e_center = numpy_helper.from_array(np.array([1,17,257,17], dtype=np.int64), "e_center")
    inits += [s_center, e_center]
    nodes.append(helper.make_node("Slice", ["padded", "s_center", "e_center", "axes4", "steps4"], ["center"]))

    zero_f = numpy_helper.from_array(np.array(0.0, dtype=np.float32), "zero_f")
    inits.append(zero_f)
    nodes.append(helper.make_node("Greater", ["center", "zero_f"], ["center_gt"]))
    nodes.append(helper.make_node("Cast", ["center_gt"], ["center_block"], to=TensorProto.FLOAT))

    face_names = []
    for i, (dirname, _, _) in enumerate(dirs):
        nodes.append(helper.make_node("Equal", [dirname, "zero_f"], [f"eq_{i}"]))
        nodes.append(helper.make_node("Cast", [f"eq_{i}"], [f"air_{i}"], to=TensorProto.FLOAT))
        nodes.append(helper.make_node("Mul", ["center_block", f"air_{i}"], [f"face_{i}"]))
        face_names.append(f"face_{i}")

    nodes.append(helper.make_node("Concat", face_names, ["face_6d"], axis=0))
    axes_unsq = numpy_helper.from_array(np.array([0], dtype=np.int64), "axes_unsq")
    inits.append(axes_unsq)
    nodes.append(helper.make_node("Unsqueeze", ["face_6d", "axes_unsq"], ["face_mask"]))
    nodes.append(helper.make_node("Identity", ["face_mask"], ["face_mask_out"]))
    nodes.append(helper.make_node("ReduceSum", ["face_mask"], ["vc_float"], keepdims=0))
    nodes.append(helper.make_node("Cast", ["vc_float"], ["vertex_count"], to=TensorProto.INT32))

    graph = helper.make_graph(nodes, "chunk_mesh_v1", [block_data], [face_mask, vertex_count], initializer=inits)
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 17)], ir_version=8)
    onnx.checker.check_model(model)
    path = os.path.join(OUTPUT_DIR, "chunk_mesh_v1.onnx")
    onnx.save(model, path)
    print(f"  {path} ({os.path.getsize(path)} bytes)")
    return model


def create_light_baking_model():
    """光照烘焙 - 简化版: max(sky_light, block_light) + 向下传播"""
    sky_in = helper.make_tensor_value_info("sky_light", TensorProto.FLOAT, [1, 16, 256, 16])
    block_in = helper.make_tensor_value_info("block_light", TensorProto.FLOAT, [1, 16, 256, 16])
    opaque_in = helper.make_tensor_value_info("opaque_mask", TensorProto.FLOAT, [1, 16, 256, 16])
    out = helper.make_tensor_value_info("combined_light", TensorProto.FLOAT, [1, 16, 256, 16])

    nodes = []
    inits = []

    one_f = numpy_helper.from_array(np.array(1.0, dtype=np.float32), "one_f")
    max_f = numpy_helper.from_array(np.array(15.0, dtype=np.float32), "max_f")
    min_f = numpy_helper.from_array(np.array(0.0, dtype=np.float32), "min_f")
    inits += [one_f, max_f, min_f]

    # --- 天空光向下传播 ---
    # Slice 当前 y 和 上方 y-1
    axes4 = numpy_helper.from_array(np.array([0,1,2,3], dtype=np.int64), "la4")
    steps4 = numpy_helper.from_array(np.array([1,1,1,1], dtype=np.int64), "ls4")
    inits += [axes4, steps4]

    # 当前层 sky_light [1,16,256,16]
    # 上方层: sky_light[:, 1:, :, :] = [1,15,256,16]
    s_cur = numpy_helper.from_array(np.array([0,1,0,0], dtype=np.int64), "s_cur")
    e_cur = numpy_helper.from_array(np.array([1,16,256,16], dtype=np.int64), "e_cur")
    s_above = numpy_helper.from_array(np.array([0,0,0,0], dtype=np.int64), "s_above")
    e_above = numpy_helper.from_array(np.array([1,15,256,16], dtype=np.int64), "e_above")
    inits += [s_cur, e_cur, s_above, e_above]

    nodes.append(helper.make_node("Slice", ["sky_light", "s_cur", "e_cur", "la4", "ls4"], ["sky_below"]))
    nodes.append(helper.make_node("Slice", ["sky_light", "s_above", "e_above", "la4", "ls4"], ["sky_above"]))

    # propagated = max(sky_below, sky_above - 1)
    nodes.append(helper.make_node("Sub", ["sky_above", "one_f"], ["sky_above_m1"]))
    nodes.append(helper.make_node("Clip", ["sky_above_m1", "min_f", "max_f"], ["sky_above_clamped"]))
    nodes.append(helper.make_node("Max", ["sky_below", "sky_above_clamped"], ["sky_prop"]))

    # Pad 回 [1,16,256,16] (第一行用原始值)
    # 合并: 第0行用原始, 第1-15行用 propagated
    # 简化: 整体用 max(sky_light, propagated) 
    # 但维度不同，需要 pad propagated 到 [1,16,256,16]
    pad_prop = numpy_helper.from_array(np.array([0,1,0,0, 0,0,0,0], dtype=np.int64), "pad_prop")
    inits.append(pad_prop)
    nodes.append(helper.make_node("Pad", ["sky_prop", "pad_prop", "min_f"], ["sky_prop_padded"]))
    # sky_prop_padded [1,16,256,16], 第0行是0
    nodes.append(helper.make_node("Max", ["sky_light", "sky_prop_padded"], ["sky_final"]))

    # --- 方块光: clamp ---
    nodes.append(helper.make_node("Clip", ["block_light", "min_f", "max_f"], ["block_clamped"]))

    # --- 合并 ---
    nodes.append(helper.make_node("Max", ["sky_final", "block_clamped"], ["combined_light"]))

    graph = helper.make_graph(nodes, "light_baking_v1", [sky_in, block_in, opaque_in], [out], initializer=inits)
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 17)], ir_version=8)
    onnx.checker.check_model(model)
    path = os.path.join(OUTPUT_DIR, "light_baking_v1.onnx")
    onnx.save(model, path)
    print(f"  {path} ({os.path.getsize(path)} bytes)")
    return model


if __name__ == "__main__":
    print("=== Generating Carbon NPU ONNX Models ===")
    create_chunk_mesh_model()
    create_light_baking_model()

    print("\n=== Verifying with ONNX Runtime ===")
    for name in ["chunk_mesh_v1.onnx", "light_baking_v1.onnx"]:
        path = os.path.join(OUTPUT_DIR, name)
        sess = ort.InferenceSession(path)
        print(f"\n{name}:")
        for i in sess.get_inputs():
            print(f"  Input:  {i.name} {i.shape} {i.type}")
        for o in sess.get_outputs():
            print(f"  Output: {o.name} {o.shape} {o.type}")

        if "chunk" in name:
            data = np.random.randint(0, 10, (1, 16, 256, 16), dtype=np.int32)
            result = sess.run(None, {"block_data": data})
            visible = np.sum(result[1])
            print(f"  Test: {visible} visible faces")
        else:
            sky = np.random.rand(1, 16, 256, 16).astype(np.float32) * 15
            block = np.random.rand(1, 16, 256, 16).astype(np.float32) * 15
            opaque = np.zeros((1, 16, 256, 16), dtype=np.float32)
            result = sess.run(None, {"sky_light": sky, "block_light": block, "opaque_mask": opaque})
            print(f"  Test: mean={np.mean(result[0]):.2f}, max={np.max(result[0]):.2f}")
    print("\n=== Done ===")
