// THIS CORNFILE IS GENERATED. DO NOT EDIT! 🌽
#ifndef __REFLECTION_INTERNALH
#define __REFLECTION_INTERNALH
#include <string>
#include <vector>
enum class ReflectTypeID {
EnumReflectTypeID = 0,
ClassReflectField = 1,
ClassReflectEnumValue = 2,
ClassReflectType = 3,
EnumReflectTypeKind = 4,
VectorOfClassReflectField = 5,
VectorOfClassReflectEnumValue = 6,
UnsignedChar = 7,
Double = 8,
Float = 9,
String = 10,
Int64 = 11,
Uint32 = 12,
Int = 13,
Bool = 14,
Int32 = 15,
Uint64 = 16,
Uint8 = 17,
Char = 18,
VectorOfUint8 = 19,
EnumCpuFamily = 20,
EnumOs = 21,
EnumAuthenticationType = 22,
ClassSystemInfo = 23,
ClassLoginCredentials = 24,
ClassClientResponseEncrypted = 25,
OptionalOfString = 26,
OptionalOfVectorOfUint8 = 27,
EnumProduct = 28,
EnumPlatform = 29,
EnumCryptosuite = 30,
ClassLoginCryptoDiffieHellmanChallenge = 31,
ClassLoginCryptoChallengeUnion = 32,
ClassLoginCryptoDiffieHellmanHello = 33,
ClassLoginCryptoHelloUnion = 34,
ClassBuildInfo = 35,
ClassFeatureSet = 36,
ClassAPChallenge = 37,
ClassAPResponseMessage = 38,
ClassLoginCryptoDiffieHellmanResponse = 39,
ClassLoginCryptoResponseUnion = 40,
ClassCryptoResponseUnion = 41,
ClassPoWResponseUnion = 42,
ClassClientResponsePlaintext = 43,
ClassClientHello = 44,
OptionalOfClassLoginCryptoDiffieHellmanChallenge = 45,
OptionalOfClassLoginCryptoDiffieHellmanHello = 46,
OptionalOfBool = 47,
OptionalOfClassAPChallenge = 48,
OptionalOfClassLoginCryptoDiffieHellmanResponse = 49,
VectorOfEnumCryptosuite = 50,
OptionalOfClassFeatureSet = 51,
ClassHeader = 52,
EnumAudioFormat = 53,
ClassAudioFile = 54,
ClassRestriction = 55,
ClassTrack = 56,
ClassEpisode = 57,
OptionalOfEnumAudioFormat = 58,
OptionalOfInt32 = 59,
VectorOfClassRestriction = 60,
VectorOfClassAudioFile = 61,
VectorOfClassTrack = 62,
EnumMessageType = 63,
EnumPlayStatus = 64,
EnumCapabilityType = 65,
ClassTrackRef = 66,
ClassState = 67,
ClassCapability = 68,
ClassDeviceState = 69,
ClassFrame = 70,
OptionalOfUint32 = 71,
OptionalOfEnumPlayStatus = 72,
OptionalOfUint64 = 73,
VectorOfClassTrackRef = 74,
OptionalOfEnumCapabilityType = 75,
VectorOfInt64 = 76,
VectorOfString = 77,
OptionalOfInt64 = 78,
VectorOfClassCapability = 79,
OptionalOfEnumMessageType = 80,
OptionalOfClassDeviceState = 81,
OptionalOfClassState = 82,
};

enum class ReflectTypeKind {
Primitive = 0,
Enum = 1,
Class = 2,
Vector = 3,
Optional = 4,
};

class ReflectField {
public:
ReflectTypeID typeID;
std::string name;
size_t offset;
uint32_t protobufTag;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassReflectField;

			ReflectField() {};
			ReflectField(ReflectTypeID typeID, std::string name, size_t offset, uint32_t protobufTag) {
				this->typeID = typeID;
				this->name = name;
				this->offset = offset;
				this->protobufTag = protobufTag;
			}
		};

class ReflectEnumValue {
public:
std::string name;
int value;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassReflectEnumValue;

			ReflectEnumValue(){};
			ReflectEnumValue( std::string name, int value) {
				this->name = name;
				this->value = value;
			}
		};

class ReflectType {
public:
ReflectTypeID typeID;
std::string name;
ReflectTypeKind kind;
size_t size;
ReflectTypeID innerType;
std::vector<ReflectField> fields;
std::vector<ReflectEnumValue> enumValues;
static constexpr ReflectTypeID _TYPE_ID = ReflectTypeID::ClassReflectType;

		void (*_Construct)(void *mem);
		void (*_Destruct)(void *obj);
		VectorOperations vectorOps;
		OptionalOperations optionalOps;
		static ReflectType ofPrimitive(ReflectTypeID id, std::string name, size_t size) {
			ReflectType t;
			t.kind = ReflectTypeKind::Primitive;
			t.typeID = id;
			t.name = name;
			t.size = size;
			return t;
		}
		static ReflectType ofEnum(ReflectTypeID id, std::string name, std::vector<ReflectEnumValue> enumValues, size_t size) {
			ReflectType t;
			t.kind = ReflectTypeKind::Enum;
			t.typeID = id;
			t.name = name;
			t.size = size;
			t.enumValues = enumValues;
			return t;
		}
		static ReflectType ofVector(ReflectTypeID id, ReflectTypeID innerType, size_t size, 
			VectorOperations vectorOps,
			void (*_Construct)(void *mem), void (*_Destruct)(void *obj)) {
			ReflectType t;
			t.kind = ReflectTypeKind::Vector;
			t.typeID = id;
			t.innerType = innerType;
			t.size = size;
			t._Construct = _Construct;
			t._Destruct = _Destruct;
			t.vectorOps = vectorOps;
			return t;
		}
		static ReflectType ofOptional(ReflectTypeID id, ReflectTypeID innerType, size_t size, 
			OptionalOperations optionalOps,
			void (*_Construct)(void *mem), void (*_Destruct)(void *obj)) {
			ReflectType t;
			t.kind = ReflectTypeKind::Optional;
			t.typeID = id;
			t.innerType = innerType;
			t.size = size;
			t._Construct = _Construct;
			t._Destruct = _Destruct;
			t.optionalOps = optionalOps;
			return t;
		}
		static ReflectType ofClass(ReflectTypeID id, std::string name, std::vector<ReflectField> fields, size_t size, void (*_Construct)(void *mem), void (*_Destruct)(void *obj)) {
			ReflectType t;
			t.kind = ReflectTypeKind::Class;
			t.name = name;
			t.typeID = id;
			t.size = size;
			t.fields = std::move(fields);
			t._Construct = _Construct;
			t._Destruct = _Destruct;
			return t;
		}
		
		};

#endif
