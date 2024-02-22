#pragma once

#include "pch.h"

using namespace SDK;

class PalInternalUtility {
public:
	static PalInternalUtility* GetInstance();

	UWorld* GetWorld();
	UKismetStringLibrary* GetKismetStringLibrary();
	UKismetSystemLibrary* GetKismetSystemLibrary();
	UPalUtility* GetPalUtility();
	UPalBaseCampManager* GetPalBaseCampManager();
	UPalCheatManager* GetPalCheatManager();
	UPalLocationManager* GetPalLocationManager();

	void Init(UWorld* World);
	void Tick();
	
	std::string FGuid2String(const FGuid& FGuid);
	std::wstring AsciiString2WString(const std::string& str);
	void SendChat(const std::wstring message);

private:
	static PalInternalUtility* instance;
	
	UWorld* World = nullptr;
	UKismetStringLibrary* KismetStringLibrary = nullptr;
	UKismetSystemLibrary* KismetSystemLibrary = nullptr;
	UPalUtility* PalUtility = nullptr;
	UPalBaseCampManager* PalBaseCampManager = nullptr;
	UPalCheatManager* PalCheatManager = nullptr;
	UPalLocationManager* PalLocationManager = nullptr;

	PalInternalUtility(); // Private constructor
	~PalInternalUtility(); // Private destructor
	PalInternalUtility(const PalInternalUtility&) = delete;
	PalInternalUtility& operator=(const PalInternalUtility&) = delete;
	PalInternalUtility(PalInternalUtility&&) = delete;
	PalInternalUtility& operator=(PalInternalUtility&&) = delete;
};

struct FGuidLess
{
	bool operator()(const FGuid& lhs, const FGuid& rhs) const
	{
		if (lhs.A ^ rhs.A) return lhs.A < rhs.A;
		if (lhs.B ^ rhs.B) return lhs.B < rhs.B;
		if (lhs.C ^ rhs.C) return lhs.C < rhs.C;
		return lhs.D < rhs.D;
	}
};
