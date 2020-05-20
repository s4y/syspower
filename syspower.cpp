#include <IOKit/IOKitLib.h>
#include <libkern/OSByteOrder.h>

#include <cstdio>

// Portions of this file are from the Chromium project and governed by a
// license found in LICENSE-chromium.txt at the root of this project. The code
// is from this file:
// <https://chromium.googlesource.com/chromium/src/+/272fabc72409d20cc2dbf2028424126d2b6edddb/chrome/browser/metrics/power_metrics_provider_mac.mm>
// I wrote the bulk of it and want it to be available as a standalone tool.


// This API is undocumented. It can read hardware sensors including
// temperature, voltage, and power. A useful tool for discovering new keys is
// <https://github.com/theopolis/smc-fuzzer>. The following definitions are
// from
// <https://opensource.apple.com/source/PowerManagement/PowerManagement-271.1.1/pmconfigd/PrivateLib.c.auto.html>.
struct SMCParamStruct {
  enum {
    kSMCUserClientOpen = 0,
    kSMCUserClientClose = 1,
    kSMCHandleYPCEvent = 2,
    kSMCReadKey = 5,
    kSMCGetKeyInfo = 9,
  };

  enum class SMCKey : uint32_t {
    TotalPower = 'PSTR',  // Power: System Total Rail (watts)
    CPUPower = 'PCPC',    // Power: CPU Package CPU (watts)
    iGPUPower = 'PCPG',   // Power: CPU Package GPU (watts)
    GPU0Power = 'PG0R',   // Power: GPU 0 Rail (watts)
    GPU1Power = 'PG1R',   // Power: GPU 1 Rail (watts)
  };

  // SMC keys are typed, and there are a number of numeric types. Support for
  // decoding the ones in this enum is implemented below, but there are more
  // types (and more may appear in future hardware). Implement as needed.
  enum class DataType : uint32_t {
    flt = 'flt ',   // Floating point
    sp78 = 'sp78',  // Fixed point: SIIIIIIIFFFFFFFF
    sp87 = 'sp87',  // Fixed point: SIIIIIIIIFFFFFFF
    spa5 = 'spa5',  // Fixed point: SIIIIIIIIIIFFFFF
  };

  struct SMCVersion {
    unsigned char major;
    unsigned char minor;
    unsigned char build;
    unsigned char reserved;
    unsigned short release;
  };

  struct SMCPLimitData {
    uint16_t version;
    uint16_t length;
    uint32_t cpuPLimit;
    uint32_t gpuPLimit;
    uint32_t memPLimit;
  };

  struct SMCKeyInfoData {
    IOByteCount dataSize;
    DataType dataType;
    uint8_t dataAttributes;
  };

  SMCKey key;
  SMCVersion vers;
  SMCPLimitData pLimitData;
  SMCKeyInfoData keyInfo;
  uint8_t result;
  uint8_t status;
  uint8_t data8;
  uint32_t data32;
  uint8_t bytes[32];
};

float FromSMCFixedPoint(uint8_t* bytes, size_t fraction_bits) {
  return static_cast<int16_t>(OSReadBigInt16(bytes, 0)) /
         static_cast<float>(1 << fraction_bits);
}

class SMCKey {
 public:
  SMCKey(io_object_t connect,
         SMCParamStruct::SMCKey key)
      : connect_(connect), key_(key) {
    SMCParamStruct out{};
    if (CallSMCFunction(SMCParamStruct::kSMCGetKeyInfo, &out))
      keyInfo_ = out.keyInfo;
  }

  bool Exists() { return keyInfo_.dataSize > 0; }

  float Read() {
    if (!Exists())
      return 0;

    SMCParamStruct out{};
    if (!CallSMCFunction(SMCParamStruct::kSMCReadKey, &out))
      return 0;
    switch (keyInfo_.dataType) {
      case SMCParamStruct::DataType::flt:
        return *reinterpret_cast<float*>(out.bytes);
      case SMCParamStruct::DataType::sp78:
        return FromSMCFixedPoint(out.bytes, 8);
      case SMCParamStruct::DataType::sp87:
        return FromSMCFixedPoint(out.bytes, 7);
      case SMCParamStruct::DataType::spa5:
        return FromSMCFixedPoint(out.bytes, 5);
      default:
        break;
    }
    return 0;
  }

 private:
  bool CallSMCFunction(uint8_t which, SMCParamStruct* out) {
    if (!connect_)
      return false;
    if (IOConnectCallMethod(connect_, SMCParamStruct::kSMCUserClientOpen,
                            nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr,
                            nullptr)) {
      connect_ = 0;
      return false;
    }

    SMCParamStruct in{};
    in.key = key_;
    in.keyInfo.dataSize = keyInfo_.dataSize;
    in.data8 = which;

    size_t out_size = sizeof(*out);
    bool success = IOConnectCallStructMethod(
                       connect_, SMCParamStruct::kSMCHandleYPCEvent, &in,
                       sizeof(in), out, &out_size) == kIOReturnSuccess;

    if (IOConnectCallMethod(connect_, SMCParamStruct::kSMCUserClientClose,
                            nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr,
                            nullptr))
      connect_ = 0;

    // Even if the close failed, report whether the actual call succeded.
    return success;
  }

  io_object_t connect_;
  SMCParamStruct::SMCKey key_;
  SMCParamStruct::SMCKeyInfoData keyInfo_{};
};

int main() {
  io_service_t service = IOServiceGetMatchingService(
      kIOMasterPortDefault, IOServiceMatching("AppleSMC"));

  io_object_t connect;
  IOServiceOpen(service, mach_task_self(), 1, &connect);


  IOConnectCallMethod(connect, SMCParamStruct::kSMCUserClientOpen,
                            NULL, 0, NULL, 0, NULL, NULL, NULL,
                            NULL);

  auto powerKey = SMCKey(connect, SMCParamStruct::SMCKey::TotalPower);

  for (;;) {
    printf("%f\n", powerKey.Read());
    fflush(stdout);
    sleep(1);
  }
}
