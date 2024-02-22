#include "pch.h"
#include "include/PalInternalUtility.hpp"

#include <sstream>
#include <iomanip>

using namespace SDK;

PalInternalUtility* PalInternalUtility::instance = nullptr;

PalInternalUtility* PalInternalUtility::GetInstance() {
	if (instance == nullptr) {
		instance = new PalInternalUtility();
	}
	return instance;
}

void PalInternalUtility::Init(UWorld* World) {
	this->World = World; // FIXME: not right
	PalUtility = UPalUtility::GetDefaultObj();
	KismetSystemLibrary = UKismetSystemLibrary::GetDefaultObj();
	KismetStringLibrary = UKismetStringLibrary::GetDefaultObj();
	PalBaseCampManager = PalUtility->GetBaseCampManager(this->World);
	PalCheatManager = PalUtility->GetPalCheatManager(this->World);
}

void PalInternalUtility::Tick() {}

UWorld* PalInternalUtility::GetWorld() {
	return World;
}

UKismetStringLibrary* PalInternalUtility::GetKismetStringLibrary() {
	return KismetStringLibrary;
}

UKismetSystemLibrary* PalInternalUtility::GetKismetSystemLibrary() {
	return KismetSystemLibrary;
}

UPalUtility* PalInternalUtility::GetPalUtility() {
	return PalUtility;
}

UPalBaseCampManager* PalInternalUtility::GetPalBaseCampManager() {
	return PalBaseCampManager;
}

UPalCheatManager* PalInternalUtility::GetPalCheatManager() {
	return PalCheatManager;
}

UPalLocationManager* PalInternalUtility::GetPalLocationManager() {
	return PalLocationManager;
}

std::string PalInternalUtility::FGuid2String(const FGuid& FGuid) {
	std::stringstream ss;
	ss << std::hex << std::setfill('0') << std::setw(8) << FGuid.A << std::setw(8) << FGuid.B << std::setw(8) << FGuid.C << std::setw(8) << FGuid.D;
	return ss.str();
}

void PalInternalUtility::SendChat(const std::wstring message) {
	// GetPalCheatManager()->SendChatToBroadcast(FString(message.c_str()));
	// above seems not work
	GetPalUtility()->SendSystemAnnounce(
		PalInternalUtility::GetInstance()->GetWorld(), 
		FString(message.c_str()));
}

PalInternalUtility::PalInternalUtility() {}
PalInternalUtility::~PalInternalUtility() {}

