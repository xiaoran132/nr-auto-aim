// 38 类装甲板标签：与 assets/yolo11.xml 的 model_info/labels 顺序完全一致。
// 注意：这是 2025 赛季 sp_vision_25 模型的类别集合，使用前请按当季官方规则核对
// （five / balance* / E / P 等类的语义与当季兵种编制）。
#pragma once

#include <array>
#include <string>

namespace nr_vision_detector
{
inline constexpr std::array<const char *, 38> kArmorLabels = {
  "Bsentry", "Rsentry", "Esentry",
  "Bone", "Rone", "Eone",
  "Btwo", "Rtwo", "Etwo",
  "Bthree", "Rthree", "Ethree",
  "Bfour", "Rfour", "Efour",
  "Bfive", "Rfive", "Efive",
  "Boutpost", "Routpost", "Eoutpost",
  "Bbase", "Rbase", "Ebase", "Pbase",
  "Bbasesmall", "Rbasesmall", "Ebasesmall", "Pbasesmall",
  "Bbalancethree", "Rbalancethree", "Ebalancethree",
  "Bbalancefour", "Rbalancefour", "Ebalancefour",
  "Bbalancefive", "Rbalancefive", "Ebalancefive"};

struct ArmorLabelInfo
{
  std::string color;  // B=blue / R=red / E=extinguish / P=purple
  std::string name;   // sentry / one..five / outpost / base / basesmall / balance*
  std::string type;   // small / large
};

inline ArmorLabelInfo parseLabel(const std::string & label)
{
  ArmorLabelInfo info;
  if (label.size() < 2) {
    return info;
  }
  info.color = label.substr(0, 1);
  info.name = label.substr(1);
  if (info.name == "base" || info.name.rfind("balance", 0) == 0) {
    info.type = "large";
  } else {
    info.type = "small";
  }
  return info;
}

inline std::string labelOf(int class_id)
{
  if (class_id < 0 || class_id >= static_cast<int>(kArmorLabels.size())) {
    return "unknown";
  }
  return kArmorLabels[static_cast<size_t>(class_id)];
}

}  // namespace nr_vision_detector
