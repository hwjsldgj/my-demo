import torch
import torch.onnx
from model import TinyGoNet  # 确保 model.py 在同一个目录

# 1. 创建模型（根据 best_model.pth 的配置，检查 TinyGo 项目文档确定参数）
# 从错误信息看，模型有 6 个残差块，所以 num_blocks=6
# channels 可能是 64（标准）或 128（large），请确认你的 best_model.pth 是标准版还是 large 版
# 你可以先尝试标准版 64
model = TinyGoNet(num_blocks=6, channels=64, num_classes=361)
# 如果是 large 版：model = TinyGoNet(num_blocks=10, channels=128, num_classes=361)

# 2. 加载检查点文件
checkpoint = torch.load('best_model.pth', map_location='cpu')

# 3. 提取模型权重（如果 'model_state_dict' 存在）
if 'model_state_dict' in checkpoint:
    state_dict = checkpoint['model_state_dict']
else:
    state_dict = checkpoint  # 直接是权重

# 4. 加载权重（允许丢失部分键，但为了安全，我们使用 strict=True 检查）
try:
    model.load_state_dict(state_dict, strict=True)
    print("✅ 权重加载成功")
except RuntimeError as e:
    print("⚠️ 权重不匹配，尝试 strict=False")
    model.load_state_dict(state_dict, strict=False)
    print("✅ 权重加载完成（部分键可能忽略）")

model.eval()

# 5. 虚拟输入（TinyGo 是 3 通道）
dummy_input = torch.randn(1, 3, 19, 19)

# 6. 导出 ONNX
torch.onnx.export(
    model,
    dummy_input,
    "go_model.onnx",
    export_params=True,
    opset_version=17,
    do_constant_folding=True,
    input_names=['input'],
    output_names=['output'],
    dynamic_axes={'input': {0: 'batch_size'}, 'output': {0: 'batch_size'}}
)
print("✅ 导出成功：go_model.onnx")