#ifdef ESPSOMFY_LOCAL_UI
#include "LocalEntityRuntime.h"
#include "LiftCalibrationJson.h"
#include "LocalOperationAdmission.h"
#include "LocalTargetRead.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LoopQueue.h"
#include "ModelCommitService.h"
#include "Somfy.h"
#include "SomfyEditor.h"
#include "Utils.h"
#include "SafeRestart.h"
#include <stdio.h>
#include <string.h>
extern SomfyShadeController somfy;
extern LoopQueue loopQueue;

namespace LocalUi {
namespace EntityAdmin {
namespace {
static_assert(SOMFY_MAX_SHADES <= 32 && SOMFY_MAX_GROUPS <= 32 &&
                  SOMFY_MAX_ROOMS <= 32,
              "Ordering covers the complete configured physical model");
class RuntimeBackend final : public Backend {
  Kind preparedKind = Kind::room;
  uint8_t sourceId = 0, offeredId = 0;
  bool prepared = false;
  RawTargetContext context;
  bool capture(uint8_t id, RawTargetContext &out) {
    return readRawTargetContext(
        somfy,
        {preparedKind == Kind::shade ? EntityKind::device : EntityKind::group,
         id},
        out);
  }
  bool currentContext() {
    if (!prepared)
      return false;
    if (preparedKind == Kind::room)
      return true;
    RawTargetContext now;
    return capture(sourceId ? sourceId : offeredId, now) && context.same(now);
  }
  template <typename Remote>
  bool readRemote(Remote *remote, proto_field_t field, Value &out) {
    switch (field) {
    case proto_field_t::remoteAddress:
      out.integer = remote ? remote->getRemoteAddress()
                           : somfy.getNextRemoteAddress(offeredId);
      break;
    case proto_field_t::roomId:
      out.integer = remote ? remote->roomId : 0;
      break;
    case proto_field_t::bitLength:
      out.integer = remote ? remote->bitLength : somfy.transceiver.config.type;
      break;
    case proto_field_t::proto:
      out.integer = remote ? static_cast<uint8_t>(remote->proto) : 0;
      break;
    case proto_field_t::repeats:
      out.integer = remote ? remote->repeats : 1;
      break;
    case proto_field_t::flipCommands:
      out.boolean = remote ? remote->flipCommands : false;
      break;
    case proto_field_t::name:
      if (remote) {
        if (!memchr(remote->name, 0, sizeof(remote->name)))
          return false;
        memcpy(out.text, remote->name, sizeof(out.text));
      }
      break;
    default:
      return false;
    }
    return true;
  }
  static ApplyResult invalid(proto_field_t field, const char *reason) {
    ApplyResult out;
    out.admission = Admission::invalid;
    out.field = field;
    snprintf(out.reason, sizeof(out.reason), "%s", reason);
    return out;
  }
  model_edit_type_t operation(bool add, bool remove = false) const {
    if (preparedKind == Kind::room)
      return remove ? model_edit_type_t::delete_room
             : add  ? model_edit_type_t::add_room
                    : model_edit_type_t::save_room;
    if (preparedKind == Kind::shade)
      return remove ? model_edit_type_t::delete_shade
             : add  ? model_edit_type_t::add_shade
                    : model_edit_type_t::save_shade;
    return remove ? model_edit_type_t::delete_group
           : add  ? model_edit_type_t::add_group
                  : model_edit_type_t::save_group;
  }
  const char *identityKey() const {
    return preparedKind == Kind::room    ? "roomId"
           : preparedKind == Kind::shade ? "shadeId"
                                         : "groupId";
  }
  ApplyResult preflight(model_edit_type_t type, JsonDocument &doc) {
    if (!prepared || (type != operation(!sourceId) &&
                      (sourceId == 0 || type != operation(false, true))))
      return {{}, Admission::stale};
    if (sourceId && (!doc[identityKey()].is<uint8_t>() ||
                     doc[identityKey()].as<uint8_t>() != sourceId))
      return {{}, Admission::stale};
    if (!doc.is<JsonObject>())
      return invalid(proto_field_t::count, "Expected an entity edit object");
    if (!sourceId && !doc[identityKey()].isNull())
      return {{}, Admission::stale};
    if (type == operation(false, true)) {
      if (preparedKind == Kind::shade &&
          somfy.getShadeById(sourceId)->isInGroup())
        return invalid(proto_field_t::count,
                       "Remove this device from groups before deleting");
      return {};
    }
    const auto *form = editForm(type);
    if (!form)
      return {{}, Admission::stale};
    // Device/group sort order is assigned by Add rather than an editable
    // scalar field. Refuse an unrepresentable hidden default, never wrap it.
    if (!sourceId && preparedKind == Kind::shade &&
        static_cast<int>(somfy.getMaxShadeOrder()) + 1 > INT8_MAX)
      return invalid(proto_field_t::count, "Reorder devices before adding");
    if (!sourceId && preparedKind == Kind::group &&
        static_cast<int>(somfy.getMaxGroupOrder()) + 1 > INT8_MAX)
      return invalid(proto_field_t::count, "Reorder groups before adding");
    // New entities receive exactly the defaults displayed to the operator.
    // Existing entities remain changed-only, preserving omitted legacy values.
    if (!sourceId) {
      Value protocol;
      if (!read(preparedKind, 0, proto_field_t::proto, protocol) &&
          preparedKind != Kind::room)
        return {{}, Admission::stale};
      const auto effective = static_cast<radio_proto>(
          doc["proto"].isNull() ? protocol.integer
                                : doc["proto"].as<int64_t>());
      for (uint8_t i = 0; i < form->count; ++i) {
        const auto field = form->fields[i];
        const auto *meta = entityField(type, field);
        const auto &schema = meta->schema();
        if (!doc[schema.name].isNull() || !entityFieldActive(*meta, effective))
          continue;
        Value value;
        if (!read(preparedKind, 0, field, value))
          return {{}, Admission::stale};
        if (schema.kind == 's')
          doc[schema.name] = value.text;
        else if (schema.kind == 'b')
          doc[schema.name] = value.boolean;
        else
          doc[schema.name] = value.integer;
        if (doc.overflowed())
          return {{}, Admission::noMemory};
      }
    }
    radio_proto effective = radio_proto::RTS;
    if (preparedKind != Kind::room) {
      Value value;
      if (!read(preparedKind, sourceId, proto_field_t::proto, value))
        return {{}, Admission::stale};
      effective = static_cast<radio_proto>(
          doc["proto"].isNull() ? value.integer : doc["proto"].as<int64_t>());
    }
    for (const auto member : doc.as<JsonObjectConst>()) {
      if (sourceId && strcmp(member.key().c_str(), identityKey()) == 0)
        continue;
      const entity_field_t *meta = nullptr;
      for (uint8_t i = 0; i < form->count; ++i) {
        const auto *candidate = entityField(type, form->fields[i]);
        if (strcmp(candidate->schema().name, member.key().c_str()) == 0) {
          meta = candidate;
          break;
        }
      }
      if (!meta)
        return invalid(proto_field_t::count, "Unsupported field in this form");
      if (!validEntityValue(*meta, member.value()))
        return invalid(meta->field,
                       "Value is outside the supported field limits");
      if (!entityFieldActive(*meta, effective))
        return invalid(meta->field, "GPIO field is inactive for this protocol");
    }
    if (preparedKind == Kind::room)
      return {};
    if (!doc["roomId"].isNull()) {
      const auto room = doc["roomId"].as<uint8_t>();
      if (room && !somfy.getRoomById(room))
        return invalid(proto_field_t::roomId,
                       "Choose an existing room or Unassigned");
    }
    if (!doc["remoteAddress"].isNull()) {
      const uint32_t address = doc["remoteAddress"];
      for (auto &shade : somfy.shades)
        if (shade.getShadeId() != 255 &&
            !(preparedKind == Kind::shade &&
              &shade == somfy.getShadeById(sourceId)) &&
            shade.getRemoteAddress() == address)
          return invalid(proto_field_t::remoteAddress,
                         "Remote identity is already used by another entity");
      for (auto &group : somfy.groups)
        if (group.getGroupId() != 255 &&
            !(preparedKind == Kind::group &&
              &group == somfy.getGroupById(sourceId)) &&
            group.getRemoteAddress() == address)
          return invalid(proto_field_t::remoteAddress,
                         "Remote identity is already used by another entity");
    }
    if (preparedKind == Kind::shade) {
      auto *shade = sourceId ? somfy.getShadeById(sourceId) : nullptr;
      if (!shade)
        for (auto &slot : somfy.shades)
          if (slot.getShadeId() == 255) {
            shade = &slot;
            break;
          }
      if (!shade)
        return {{}, Admission::stale};
      const auto category =
          doc["shadeType"].isNull()
              ? shade->shadeType
              : static_cast<shade_types>(doc["shadeType"].as<uint8_t>());
      const auto tilt =
          doc["tiltType"].isNull()
              ? shade->tiltType
              : static_cast<tilt_types>(doc["tiltType"].as<uint8_t>());
      if (category != shade_types::blind && tilt != tilt_types::none)
        return invalid(proto_field_t::tiltType,
                       "Set tilt to None for a device without blind tilt");
      LiftCalibration projected;
      if(!projectLiftCalibration(doc.as<JsonObjectConst>(), *shade, projected))
        return invalid(proto_field_t::calibrationKind, "Review travel times and calibration phases");
      const uint8_t up = doc["gpioUp"] | shade->gpioUp;
      const uint8_t down = doc["gpioDown"] | shade->gpioDown;
      const uint8_t my = doc["gpioMy"] | shade->gpioMy;
      const auto code = shade->validateGPIOs(effective, up, down, my);
      if (code) {
        const auto field = !doc["gpioUp"].isNull()     ? proto_field_t::gpioUp
                           : !doc["gpioDown"].isNull() ? proto_field_t::gpioDown
                           : !doc["gpioMy"].isNull()   ? proto_field_t::gpioMy
                                                       : proto_field_t::proto;
        const char *reason =
            code == PIN_STORAGE_UNAVAILABLE
                ? "GPIO ownership settings are unavailable"
            : code == PIN_CONFLICT_RADIO
                ? "GPIO is reserved by the board or radio"
            : code == PIN_CONFLICT_ETHERNET ? "GPIO is used by Ethernet"
            : code == PIN_CONFLICT_SHADE    ? "GPIO is used by another device"
            : code == PIN_CONFLICT_DUPLICATE
                ? "GPIO outputs must use distinct pads"
                : "Choose a valid output GPIO or None";
        return invalid(field, reason);
      }
    }
    return {};
  }
  bool idleForMutation() {
    if (!sourceId || preparedKind == Kind::room)
      return true;
    if (preparedKind == Kind::shade) {
      auto *shade = somfy.getShadeById(sourceId);
      return shade && shade->idleForAdministration();
    }
    // Inspect every captured physical member. Missing entries remain explicit
    // in the raw context and are administrable; never erase or compress them.
    for (uint8_t i = 0; i < context.slotCount; ++i) {
      if (!context.members[i].exists)
        continue;
      auto *shade = somfy.getShadeById(context.members[i].id);
      if (!shade || !shade->idleForAdministration())
        return false;
    }
    return true;
  }
  Admission admitAuthority(Authorization expected, uint64_t revision) {
    const auto now = authorization();
    if (!expected.allowed || !now.allowed)
      return Admission::locked;
    if (!expected.generation || !expected.configurationGeneration ||
        !revision || expected.generation != now.generation ||
        expected.configurationGeneration != now.configurationGeneration ||
        revision != this->revision())
      return Admission::stale;
    if (loopQueue.pending() || restartPending())
      return Admission::busy;
    return Admission::accepted;
  }

  Admission admit(Authorization expected, uint64_t revision) {
    const auto result = admitAuthority(expected, revision);
    if (result != Admission::accepted && result != Admission::busy)
      return result;
    return currentContext() ? result : Admission::stale;
  }
  uint8_t orderCapacity(Kind kind) const {
    if (kind == Kind::room)
      return sizeof(somfy.rooms) / sizeof(somfy.rooms[0]);
    if (kind == Kind::shade)
      return sizeof(somfy.shades) / sizeof(somfy.shades[0]);
    if (kind == Kind::group)
      return sizeof(somfy.groups) / sizeof(somfy.groups[0]);
    return 0;
  }
  template <typename Item>
  bool orderRecord(Item &item, uint8_t id, uint8_t &outId, int8_t &order,
                   Value *label) {
    outId = id;
    order = item.sortOrder;
    if (label) {
      *label = {};
      label->kind = 's';
      if (!memchr(item.name, 0, sizeof(item.name)))
        return false;
      memcpy(label->text, item.name, sizeof(label->text));
    }
    return true;
  }
  bool orderRecord(Kind kind, uint8_t slot, uint8_t &id, int8_t &order,
                   Value *label = nullptr) {
    if (slot >= orderCapacity(kind))
      return false;
    if (kind == Kind::room)
      return orderRecord(somfy.rooms[slot], somfy.rooms[slot].roomId, id, order,
                         label);
    if (kind == Kind::shade)
      return orderRecord(somfy.shades[slot], somfy.shades[slot].getShadeId(),
                         id, order, label);
    if (kind == Kind::group)
      return orderRecord(somfy.groups[slot], somfy.groups[slot].getGroupId(),
                         id, order, label);
    return false;
  }
  bool fullPermutation(Kind kind, const uint8_t *ids, uint8_t count) {
    OrderIds live{}, slots{};
    uint8_t total = 0;
    bool normalize = false;
    if (!ids || count > 32 || !readOrder(kind, live, slots, total, normalize) ||
        count != total)
      return false;
    for (uint8_t i = 0; i < count; ++i) {
      bool found = false;
      for (uint8_t j = 0; j < count; ++j)
        found |= ids[i] == live[j];
      if (!found)
        return false;
      for (uint8_t j = 0; j < i; ++j)
        if (ids[j] == ids[i])
          return false;
    }
    return true;
  }

public:
  Authorization authorization() override {
    const auto state = pollAuthorization();
    return {state.allowed && !localControlsFrozen(), state.generation,
            state.configurationGeneration};
  }
  uint64_t revision() override { return somfy.configurationRevision; }
  bool prepare(Kind kind, uint8_t requested, uint8_t &id) override {
    prepared = false;
    context = {};
    if (!authorization().allowed || requested == 255)
      return false;
    switch (kind) {
    case Kind::room:
      if (requested && !somfy.getRoomById(requested))
        return false;
      id = requested ? requested : somfy.getNextRoomId();
      break;
    case Kind::shade:
      if (requested && !somfy.getShadeById(requested))
        return false;
      id = requested ? requested : somfy.getNextShadeId();
      break;
    case Kind::group:
      if (requested && !somfy.getGroupById(requested))
        return false;
      id = requested ? requested : somfy.getNextGroupId();
      break;
    default:
      return false;
    }
    if (!id || id == 255)
      return false;
    preparedKind = kind;
    sourceId = requested;
    offeredId = id;
    if (kind != Kind::room && !capture(id, context))
      return false;
    prepared = true;
    return true;
  }
  bool read(Kind kind, uint8_t id, proto_field_t field, Value &out) override {
    out = {};
    if (!authorization().allowed)
      return false;
    out.kind = protoField(field).kind;
    if (kind == Kind::room) {
      auto *room = id ? somfy.getRoomById(id) : nullptr;
      if (id && !room)
        return false;
      if (field == proto_field_t::name) {
        if (room) {
          if (!memchr(room->name, 0, sizeof(room->name)))
            return false;
          memcpy(out.text, room->name, sizeof(out.text));
        }
      } else if (field == proto_field_t::sortOrder)
        out.integer = room ? room->sortOrder : somfy.getMaxRoomOrder() + 1;
      else
        return false;
      return true;
    }
    if (!prepared || kind != preparedKind || id != sourceId ||
        !currentContext())
      return false;
    if (kind == Kind::group) {
      auto *group = id ? somfy.getGroupById(id) : nullptr;
      return (!id || group) && readRemote(group, field, out);
    }
    if (kind != Kind::shade)
      return false;
    auto *shade = id ? somfy.getShadeById(id) : nullptr;
    if (id && !shade)
      return false;
    if (readRemote(shade, field, out))
      return true;
    const ShadeSettings defaults;
    const ShadeSettings &settings = shade ? *shade : defaults;
    switch (field) {
    case proto_field_t::shadeType:
      out.integer = static_cast<uint8_t>(settings.shadeType);
      break;
    case proto_field_t::tiltType:
      out.integer = static_cast<uint8_t>(settings.tiltType);
      break;
    case proto_field_t::calibrationKind:
      out.integer = static_cast<uint8_t>(settings.calibration.kind);
      break;
    case proto_field_t::upTakeupMs:
      out.integer = settings.calibration.up.takeupMs;
      break;
    case proto_field_t::upMidpointMs:
      out.integer = settings.calibration.up.midpointMs;
      break;
    case proto_field_t::upSettlingMs:
      out.integer = settings.calibration.up.settlingMs;
      break;
    case proto_field_t::upReversalMs:
      out.integer = settings.calibration.up.reversalMs;
      break;
    case proto_field_t::downTakeupMs:
      out.integer = settings.calibration.down.takeupMs;
      break;
    case proto_field_t::downMidpointMs:
      out.integer = settings.calibration.down.midpointMs;
      break;
    case proto_field_t::downSettlingMs:
      out.integer = settings.calibration.down.settlingMs;
      break;
    case proto_field_t::downReversalMs:
      out.integer = settings.calibration.down.reversalMs;
      break;
    case proto_field_t::upTime:
      out.integer = settings.upTime;
      break;
    case proto_field_t::downTime:
      out.integer = settings.downTime;
      break;
    case proto_field_t::tiltTime:
      out.integer = settings.tiltTime;
      break;
    case proto_field_t::stepSize:
      out.integer = settings.stepSize;
      break;
    case proto_field_t::sunSensor:
      out.boolean = settings.sunSensor;
      break;
    case proto_field_t::flipPosition:
      out.boolean = settings.flipPosition;
      break;
    case proto_field_t::simMy:
      out.boolean = settings.simMy;
      break;
    case proto_field_t::light:
      out.boolean = shade && shade->light;
      break;
    case proto_field_t::gpioUp:
      out.integer = shade ? shade->gpioUp : 255;
      break;
    case proto_field_t::gpioDown:
      out.integer = shade ? shade->gpioDown : 255;
      break;
    case proto_field_t::gpioMy:
      out.integer = shade ? shade->gpioMy : 255;
      break;
    case proto_field_t::gpioLLTrigger:
      out.boolean = shade && (shade->gpioFlags & 1);
      break;
    default:
      return false;
    }
    return true;
  }
  uint8_t choiceCount(Kind kind, uint8_t id, proto_field_t field) override {
    if (field != proto_field_t::roomId || kind == Kind::room)
      return Backend::choiceCount(kind, id, field);
    if (!authorization().allowed || !prepared || preparedKind != kind ||
        sourceId != id || !currentContext())
      return 0;
    uint8_t count = 1;
    for (uint8_t room = 1; room <= SOMFY_MAX_ROOMS; ++room)
      if (somfy.getRoomById(room))
        ++count;
    return count;
  }
  bool choiceAt(Kind kind, uint8_t id, proto_field_t field, uint8_t index,
                Choice &out) override {
    if (field != proto_field_t::roomId || kind == Kind::room)
      return Backend::choiceAt(kind, id, field, index, out);
    out = {};
    if (!choiceCount(kind, id, field))
      return false;
    if (!index) {
      strcpy(out.label, "Unassigned");
      return true;
    }
    for (uint8_t room = 1; room <= SOMFY_MAX_ROOMS; ++room) {
      const auto *entry = somfy.getRoomById(room);
      if (!entry || --index)
        continue;
      if (!memchr(entry->name, 0, sizeof(entry->name)))
        return false;
      out.value = room;
      snprintf(out.label, sizeof(out.label), "#%u %s", room, entry->name);
      return true;
    }
    return false;
  }
  ApplyResult apply(model_edit_type_t type, JsonDocument &expanded,
                    Authorization expected, uint64_t revision) override {
    const auto admission = admit(expected, revision);
    if (admission != Admission::accepted)
      return {{}, admission};
    if (!idleForMutation())
      return {{}, Admission::busy};
    if (expanded.overflowed())
      return {{}, Admission::noMemory};
    const auto checked = preflight(type, expanded);
    if (checked.admission != Admission::accepted)
      return checked;
    // Re-read auth, history, raw membership and queue after all allocations and
    // validation, immediately before entering the loop-owned domain service.
    const auto finalAdmission = admit(expected, revision);
    if (finalAdmission != Admission::accepted)
      return {{}, finalAdmission};
    if (!idleForMutation())
      return {{}, Admission::busy};
    model_edit_t edit;
    edit.type = type;
    // ADL swap moves the allocator with its storage. No copy into an internal
    // heap-backed document occurs; edit destruction releases/wipes the draft.
    swap(edit.doc, expanded);
    const auto result = applyModelEdit(edit);
    // An applied edit is never replayed on file failure. Adopt its stable ID
    // and resulting physical context, including a delete tombstone, for the
    // explicit file-only retry.
    if (result.applied && preparedKind != Kind::room) {
      sourceId = offeredId = result.id;
      if (!capture(sourceId, context))
        prepared = false;
    }
    return {result};
  }
  CommitResult retryFileCommit(Authorization expected,
                               uint64_t revision) override {
    const auto admission = admit(expected, revision);
    if (admission != Admission::accepted)
      return {{}, admission};
    return {applyModelCommit().file};
  }
  bool readOrder(Kind kind, OrderIds &ids, OrderIds &slots, uint8_t &count,
                 bool &normalize) override {
    memset(ids, 0, sizeof(ids));
    memset(slots, 0, sizeof(slots));
    count = 0;
    normalize = false;
    const uint8_t capacity = orderCapacity(kind);
    if (!authorization().allowed || !capacity || capacity > 32)
      return false;
    struct Key {
      uint8_t id, slot;
      int8_t order;
    };
    Key keys[32]{};
    uint8_t total = 0;
    for (uint8_t slot = 0; slot < capacity; ++slot) {
      uint8_t id;
      int8_t order;
      if (!orderRecord(kind, slot, id, order))
        return false;
      if ((kind == Kind::room && !id) || (kind != Kind::room && id == 255))
        continue;
      if (!id || id == 255)
        return false;
      for (uint8_t i = 0; i < total; ++i)
        if (keys[i].id == id)
          return false;
      uint8_t at = total;
      while (at && (keys[at - 1].order > order ||
                    (keys[at - 1].order == order && keys[at - 1].id > id))) {
        keys[at] = keys[at - 1];
        --at;
      }
      keys[at] = {id, slot, order};
      ++total;
    }
    for (uint8_t i = 0; i < total; ++i) {
      ids[i] = keys[i].id;
      slots[i] = keys[i].slot;
      normalize |= keys[i].order != i;
    }
    count = total;
    return true;
  }
  bool readOrderLabel(Kind kind, uint8_t slot, uint8_t expectedId,
                      Value &out) override {
    out = {};
    uint8_t actual = 0;
    int8_t order;
    if (!authorization().allowed || !expectedId || expectedId == 255 ||
        !orderRecord(kind, slot, actual, order, &out) || actual != expectedId) {
      out = {};
      return false;
    }
    return true;
  }
  ApplyResult applyOrder(Kind kind, const uint8_t *ids, uint8_t count,
                         ArduinoJson::Allocator &allocator,
                         Authorization expected, uint64_t revision) override {
    auto admission = admitAuthority(expected, revision);
    if (admission != Admission::accepted)
      return {{}, admission};
    if (!fullPermutation(kind, ids, count)) {
      admission = admitAuthority(expected, revision);
      if (admission != Admission::accepted)
        return {{}, admission};
      return invalid(proto_field_t::count,
                     "Order must include every current ID exactly once");
    }
    JsonDocument request(&allocator);
    auto array = request.to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i)
      array.add(ids[i]);
    if (request.overflowed())
      return {{}, Admission::noMemory};
    // Validate again after allocation, then read fresh authority immediately
    // before the existing loop-owned mutation and complete fanout service.
    const bool permutation = fullPermutation(kind, ids, count);
    admission = admitAuthority(expected, revision);
    if (admission != Admission::accepted)
      return {{}, admission};
    if (!permutation)
      return invalid(proto_field_t::count,
                     "Order must include every current ID exactly once");
    model_edit_t edit;
    edit.type = kind == Kind::room    ? model_edit_type_t::room_order
                : kind == Kind::shade ? model_edit_type_t::shade_order
                                      : model_edit_type_t::group_order;
    swap(edit.doc, request);
    return {applyModelEdit(edit)};
  }
  CommitResult retryOrderFile(Authorization expected,
                              uint64_t revision) override {
    const auto admission = admitAuthority(expected, revision);
    if (admission != Admission::accepted)
      return {{}, admission};
    return {applyModelCommit().file};
  }
  void activity() override { authorizationActivity(); }
};
static_assert(sizeof(RuntimeBackend) <= 640,
              "One bounded prepared entity context");
} // namespace
Backend &runtimeBackend() {
  static RuntimeBackend backend;
  return backend;
}
} // namespace EntityAdmin
} // namespace LocalUi
#endif
