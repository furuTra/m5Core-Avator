#include "AvatarDefinition.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include <ArduinoJson.h>

namespace {
// 対応する定義ファイル形式のバージョン（tools/build_avatar.py の SCHEMA_VERSION と一致させる）。
constexpr int kSupportedSchemaVersion = 1;

// config で省略されたキーの既定値（docs/avatar-definition.md の「既定値」列、
// tools/avatar_common.py と一致させる）。
constexpr const char* kDefaultBackground = "#0A0F14";
constexpr float kDefaultGazeReachX = 160.0f;
constexpr float kDefaultGazeReachY = 120.0f;
constexpr unsigned long kDefaultBlinkIntervalMinMs = 2500;
constexpr unsigned long kDefaultBlinkIntervalMaxMs = 6000;
constexpr unsigned long kDefaultBlinkClosedMs = 180;
constexpr float kDefaultGroupSmooth = 0.2f;

// smooth の上限（1.0 で補間なし）。これを超えると GazeOffset::update が目標を行き過ぎて振動・発散する。
constexpr float kMaxGroupSmooth = 1.0f;

// 定義ファイルで [X, Y] / [最小, 最大] の組を表す配列の添字。
constexpr int kXIndex = 0;
constexpr int kYIndex = 1;
constexpr int kMinIndex = 0;
constexpr int kMaxIndex = 1;

// 色の表記 "#RRGGBB" の文字数と、RRGGBB 部分の基数。
constexpr size_t kColorTextLength = 7;
constexpr int kColorRadix = 16;

bool tryReadRect(const JsonObjectConst source, AvatarRect& rect) {
  if (!source["x"].is<int>() || !source["y"].is<int>() || !source["w"].is<int>() ||
      !source["h"].is<int>()) {
    return false;
  }
  rect.x = source["x"];
  rect.y = source["y"];
  rect.w = source["w"];
  rect.h = source["h"];
  return true;
}

// "#RRGGBB" を 0xRRGGBB へ変換する。形式が違えば false。
bool tryParseColor(const char* text, uint32_t& rgb) {
  if (text == nullptr || text[0] != '#' || std::strlen(text) != kColorTextLength) {
    return false;
  }
  // strtoul は先頭の空白や符号も受け付けてしまう（"#-00001" が通る）ため、16進数字だけかを先に確かめる。
  const char* const digits = text + 1;
  for (const char* p = digits; *p != '\0'; ++p) {
    if (!std::isxdigit(static_cast<unsigned char>(*p))) {
      return false;
    }
  }
  rgb = static_cast<uint32_t>(std::strtoul(digits, nullptr, kColorRadix));
  return true;
}

int findGroup(const std::vector<AvatarGroup>& groups, const char* name) {
  for (size_t i = 0; i < groups.size(); ++i) {
    if (groups[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

// 親をたどると循環に入るグループの添字を返す（無ければ -1）。循環があると描画時に親をたどる処理
// （FaceRenderer::totalOffset）が終わらないため、読み込み時に弾く。循環が無ければどのグループから
// たどってもグループ数以内の段数で親なし(-1)に着くので、それを超えたら循環とみなす。
int findCyclicGroup(const std::vector<AvatarGroup>& groups) {
  const size_t groupCount = groups.size();
  for (size_t start = 0; start < groupCount; ++start) {
    int current = groups[start].parent;
    size_t depth = 0;
    while (current >= 0) {
      ++depth;
      if (depth > groupCount) {
        return static_cast<int>(start);
      }
      current = groups[static_cast<size_t>(current)].parent;
    }
  }
  return -1;
}

// レイヤー名の一覧を読む。一覧が空なら out を変更せず false。
bool tryReadLayers(const JsonArrayConst names, const JsonObjectConst offsets,
                   std::vector<AvatarLayer>& out) {
  if (names.isNull() || names.size() == 0) {
    return false;
  }
  for (const JsonVariantConst name : names) {
    AvatarLayer layer;
    layer.name = name.as<const char*>();
    const JsonObjectConst offset = offsets[layer.name.c_str()];
    if (!offset.isNull()) {
      layer.offsetX = offset["x"] | 0;
      layer.offsetY = offset["y"] | 0;
    }
    out.push_back(layer);
  }
  return true;
}
}  // namespace

bool AvatarDefinition::fail(const String& message) {
  error = message;
  return false;
}

bool AvatarDefinition::load(fs::FS& fs, const char* path) {
  fs::File file = fs.open(path, "r");
  if (!file) {
    return fail(String("no ") + path);
  }

  JsonDocument doc;
  const DeserializationError parseError = deserializeJson(doc, file);
  file.close();
  if (parseError) {
    return fail(String("json: ") + parseError.c_str());
  }

  const JsonObjectConst config = doc["config"];
  const JsonObjectConst measured = doc["measured"];
  if (config.isNull() || measured.isNull()) {
    return fail("no config/measured");
  }
  if ((measured["schemaVersion"] | 0) != kSupportedSchemaVersion) {
    return fail("schema not supported (run build_avatar.py)");
  }

  avatarName = measured["avatar"] | "";
  return loadDisplay(config, measured) && loadGaze(config, measured) &&
         loadGroups(config, measured) && loadStages(measured) && loadFaces(measured);
}

bool AvatarDefinition::loadDisplay(const JsonObjectConst config, const JsonObjectConst measured) {
  const JsonObjectConst canvas = measured["canvas"];
  faceSize = canvas["faceSize"] | 0;
  faceOriginX = canvas["faceOrigin"]["x"] | 0;
  faceOriginY = canvas["faceOrigin"]["y"] | 0;
  if (faceSize <= 0) {
    return fail("measured.canvas");
  }
  if (!tryParseColor(config["display"]["background"] | kDefaultBackground, backgroundRgb888)) {
    return fail("display.background");
  }
  return true;
}

bool AvatarDefinition::loadGaze(const JsonObjectConst config, const JsonObjectConst measured) {
  const JsonObjectConst gazeCenter = measured["gazeCenter"];
  if (!gazeCenter["x"].is<float>() || !gazeCenter["y"].is<float>()) {
    return fail("measured.gazeCenter");
  }
  gazeCenterX = gazeCenter["x"];
  gazeCenterY = gazeCenter["y"];
  gazeReachX = config["gazeReach"][kXIndex] | kDefaultGazeReachX;
  gazeReachY = config["gazeReach"][kYIndex] | kDefaultGazeReachY;

  const JsonObjectConst blink = config["blink"];
  blinkIntervalMinMs = blink["intervalMs"][kMinIndex] | kDefaultBlinkIntervalMinMs;
  blinkIntervalMaxMs = blink["intervalMs"][kMaxIndex] | kDefaultBlinkIntervalMaxMs;
  blinkClosedMs = blink["closedMs"] | kDefaultBlinkClosedMs;
  if (gazeReachX <= 0.0f || gazeReachY <= 0.0f || blinkIntervalMaxMs < blinkIntervalMinMs) {
    return fail("gazeReach/blink");
  }
  return true;
}

bool AvatarDefinition::loadGroups(const JsonObjectConst config, const JsonObjectConst measured) {
  // 可動量・追従は config、クリップ矩形は measured から読む。
  const JsonObjectConst configGroups = config["groups"];
  for (const JsonPairConst pair : measured["groups"].as<JsonObjectConst>()) {
    AvatarGroup group;
    group.name = pair.key().c_str();
    const JsonObjectConst groupConfig = configGroups[pair.key()];
    group.maxX = groupConfig["max"][kXIndex] | 0.0f;
    group.maxY = groupConfig["max"][kYIndex] | 0.0f;
    group.smooth = groupConfig["smooth"] | kDefaultGroupSmooth;
    // 0 以下では追従せず、上限超えでは発散する。否定形で書くのは NaN も弾くため。
    if (!(group.smooth > 0.0f && group.smooth <= kMaxGroupSmooth)) {
      return fail("smooth: " + group.name);
    }
    for (const JsonObjectConst clipSource : pair.value()["clipRects"].as<JsonArrayConst>()) {
      AvatarRect rect;
      if (!tryReadRect(clipSource, rect)) {
        return fail("clipRects: " + group.name);
      }
      group.clipRects.push_back(rect);
    }
    groups.push_back(group);
  }
  return resolveGroupParents(configGroups);
}

bool AvatarDefinition::resolveGroupParents(const JsonObjectConst configGroups) {
  // 親はグループ名で書かれているため、全グループを読んでから添字へ解決する。
  for (AvatarGroup& group : groups) {
    const char* const parentName = configGroups[group.name.c_str()]["parent"];
    if (parentName == nullptr) {
      continue;
    }
    group.parent = findGroup(groups, parentName);
    if (group.parent < 0) {
      return fail("parent: " + group.name);
    }
  }
  const int cyclicGroup = findCyclicGroup(groups);
  if (cyclicGroup >= 0) {
    return fail("parent cycle: " + groups[static_cast<size_t>(cyclicGroup)].name);
  }
  return true;
}

bool AvatarDefinition::loadStages(const JsonObjectConst measured) {
  hasBand = !measured["band"].isNull();
  if (hasBand && !tryReadRect(measured["band"], band)) {
    return fail("measured.band");
  }
  for (const JsonObjectConst source : measured["stages"].as<JsonArrayConst>()) {
    AvatarStage stage;
    if (!tryReadStage(source, stage)) {
      return false;
    }
    stages.push_back(stage);
  }
  if (!stages.empty() && !hasBand) {
    return fail("stages without band");
  }
  return true;
}

bool AvatarDefinition::tryReadStage(const JsonObjectConst source, AvatarStage& stage) {
  AvatarStage parsed;
  const char* const kind = source["kind"] | "";
  if (std::strcmp(kind, "below") == 0) {
    parsed.kind = AvatarStageKind::Below;
    for (const JsonVariantConst name : source["layers"].as<JsonArrayConst>()) {
      parsed.layers.push_back(name.as<const char*>());
    }
    stage = parsed;
    return true;
  }

  if (std::strcmp(kind, "moving") == 0) {
    parsed.kind = AvatarStageKind::Moving;
    parsed.group = findGroup(groups, source["group"] | "");
    if (parsed.group < 0) {
      return fail("stage group");
    }
  } else if (std::strcmp(kind, "overlay") == 0) {
    parsed.kind = AvatarStageKind::Overlay;
  } else {
    return fail(String("stage kind: ") + kind);
  }
  parsed.file = source["file"] | "";
  if (parsed.file.isEmpty() || !tryReadRect(source["rect"], parsed.rect)) {
    return fail("stage file/rect");
  }
  stage = parsed;
  return true;
}

bool AvatarDefinition::loadFaces(const JsonObjectConst measured) {
  const JsonObjectConst faces = measured["faces"];
  if (!tryReadLayers(faces["open"]["layers"], JsonObjectConst(), openFace) ||
      !tryReadLayers(faces["closed"]["layers"], faces["closed"]["offsets"], closedFace)) {
    return fail("measured.faces");
  }
  return true;
}
