#include "pch.h"
#include "include/AbnormalWorkerMonitor.hpp"
#include "include/CircularBuffer.hpp"
#include "include/PalInternalUtility.hpp"
#include "include/Menu.hpp"

#include <sstream>
#include <cassert>

using namespace SDK;
using namespace DX11_Base;

static std::set<FGuid, FGuidLess> base_camp_id_set;
static std::map<FGuid, UPalLocationPointBaseCamp*, FGuidLess> base_camp_id_2_location_point;
static std::map<FGuid, FVector, FGuidLess> base_camp_id_2_location;
static std::map<FGuid, FQuat, FGuidLess> base_camp_id_2_rotation;

bool IsSanityLow(APalCharacter* PalCharacter)
{
	if (!Config.IsAwmReasonIsSanityLowEnabled)
	{
		return false;
	}
	UPalUtility* PalUtility = PalInternalUtility::GetInstance()->GetPalUtility();
	UPalIndividualCharacterParameter* PalIndividualCharacterParameter = PalUtility->GetIndividualCharacterParameterByActor(PalCharacter);
	float sanity = PalIndividualCharacterParameter->GetSanityValue();
	return sanity < 50.0;
}

bool IsHungry(APalCharacter* PalCharacter)
{
	if (!Config.IsAwmReasonIsHungryEnabled)
	{
		return false;
	}
	UPalUtility* PalUtility = PalInternalUtility::GetInstance()->GetPalUtility();
	UPalIndividualCharacterParameter* PalIndividualCharacterParameter = PalUtility->GetIndividualCharacterParameterByActor(PalCharacter);
	EPalStatusHungerType PalHungerType = PalIndividualCharacterParameter->GetHungerType();
	return EPalStatusHungerType::Default != PalHungerType;
}

bool IsOutside(APalCharacter* PalCharacter)
{
	if (!Config.IsAwmReasonIsOutsideEnabled)
	{
		return false;
	}
	UPalBaseCampManager* PalBaseCampManager = PalInternalUtility::GetInstance()->GetPalBaseCampManager();
	FVector Location = PalCharacter->K2_GetActorLocation();
	return nullptr == PalBaseCampManager->GetInRangedBaseCamp(Location, .0);
}

bool IsLocHigh(APalCharacter* PalCharacter)
{
	if (!Config.IsAwmReasonIsLocHighEnabled)
	{
		return false;
	}
	UPalUtility* PalUtility = PalInternalUtility::GetInstance()->GetPalUtility();
	UPalIndividualCharacterParameter* PalIndividualCharacterParameter = PalUtility->GetIndividualCharacterParameterByActor(PalCharacter);
	FVector PalLocation = PalCharacter->K2_GetActorLocation();
	FGuid BaseCampId = PalIndividualCharacterParameter->GetBaseCampId();
	FVector BaseCampLocation = base_camp_id_2_location[BaseCampId];
	return PalLocation.Z - BaseCampLocation.Z > Config.AwmReasonIsLocHighThreshold;
}

bool isLocLow(APalCharacter* PalCharacter)
{
	if (!Config.IsAwmReasonIsLocLowEnabled)
	{
		return false;
	}
	UPalUtility* PalUtility = PalInternalUtility::GetInstance()->GetPalUtility();
	UPalIndividualCharacterParameter* PalIndividualCharacterParameter = PalUtility->GetIndividualCharacterParameterByActor(PalCharacter);
	FVector PalLocation = PalCharacter->K2_GetActorLocation();
	FGuid BaseCampId = PalIndividualCharacterParameter->GetBaseCampId();
	FVector BaseCampLocation = base_camp_id_2_location[BaseCampId];
	return BaseCampLocation.Z - PalLocation.Z > Config.AwmReasonIsLocLowThreshold;
}

TimeWindow::TimeWindow(std::string name, double duration, unsigned threshold, double interval, bool (*check_hit)(APalCharacter*))
	: name(name)
	, duration(duration)
	, threshold(threshold)
	, interval(interval)
	, check_hit(check_hit)
	, records(threshold)
{}

bool TimeWindow::is_ready_to_emit()
{
	return records.size() == records.capacity();
}

void TimeWindow::on_hit(double time)
{
	assert(!is_ready_to_emit());
	while (records.size() && records.front() + duration < time)
	{
		g_Console->printdbg("pop time %f\n", Console::Colors::blue, records.front());
		records.pop();
	}
	g_Console->printdbg("push time %f\n", Console::Colors::blue, time);
	records.push(time);
}

bool TimeWindow::is_tick_ready_for_next_interval(double time)
{
	if (records.size() && records.front() + interval * records.size() > time)
	{
		return false;
	}
	return true;
}

bool TimeWindow::try_emit()
{
	if (is_ready_to_emit())
	{
		records.clear();
		return true;
	}
	return false;
}

void TimeWindow::alongside_emit()
{
	records.clear();
}

SingleAbnormalWorkerMonitor::SingleAbnormalWorkerMonitor(APalCharacter* PalCharacter, std::vector<TimeWindow> time_windows, double cooldown)
	: PalCharacter(PalCharacter)
	, time_windows(time_windows)
	, cooldown(cooldown)
{}

void SingleAbnormalWorkerMonitor::on_tick(double time, double delta)
{
	if (time - cooldown_start < cooldown)
		return;
	std::vector<std::string> emit_reasons;
	for (auto& time_window : time_windows)
	{
		if (time_window.is_tick_ready_for_next_interval(time) && time_window.check_hit(PalCharacter))
		{
			time_window.on_hit(time);

			FVector Location = PalCharacter->K2_GetActorLocation();
			char buffer[1024];
			std::snprintf(buffer, sizeof(buffer), "%s at (%.2f %.2f %.2f) hit %s with count %d\n",
				PalCharacter->GetName().c_str(),
				Location.X, Location.Y, Location.Z,
				time_window.name.c_str(), time_window.records.size());
			g_Console->printdbg(buffer, Console::Colors::green);
			Logger::GetInstance().AddLog(buffer);
		}
		if (time_window.try_emit())
		{
			emit_reasons.push_back(time_window.name);
		}
	}
	// emit
	if (emit_reasons.size())
	{
		cooldown_start = time;
		for (auto& time_window : time_windows)
		{
			time_window.alongside_emit();
		}

		// teleport around spawn point
		UPalUtility* PalUtility = PalInternalUtility::GetInstance()->GetPalUtility();
		UPalIndividualCharacterParameter* PalIndividualCharacterParameter = PalUtility->GetIndividualCharacterParameterByActor(PalCharacter);
		FGuid BaseCampId = PalIndividualCharacterParameter->GetBaseCampId();
		FVector BaseCampLocation = base_camp_id_2_location[BaseCampId];
		FVector SpawnLocation = PalCharacter->SpawnLocation_ForServer;
		FVector Location = BaseCampLocation;
		Location.Z = BaseCampLocation.Z + 200;
		FQuat Rotation = base_camp_id_2_rotation[BaseCampId];
		// PalUtility->TeleportAroundLoccation(PalCharacter, Location, Rotation);
		PalCharacter->K2_SetActorLocation(Location, false, nullptr, true);

		std::stringstream ss;
		for (auto& reason : emit_reasons)
		{
			ss << reason << ", ";
		}
		char buffer[1024];
		std::snprintf(buffer, sizeof(buffer), "%s emit %s\n",
			PalCharacter->GetName().c_str(),
			ss.str().c_str());
		g_Console->printdbg(buffer, Console::Colors::green);
		Logger::GetInstance().AddLog(buffer);
	}
}

void AbnormalWorkerMonitor::init() {
	init_time_windows_and_cooldown();
	update_base_camps();
	update_base_camp_pals();
	init_check_spawn_point();
}

void AbnormalWorkerMonitor::on_tick(double time)
{
	if (!Config.IsAbormalWorkerMonitorEnabled)
	{
		return;
	}

	std::unique_lock<std::mutex> lock(tick_mutex);
	tick_cond_var.wait(lock, [this] { return tick_ready; });
	tick_ready = false;
	double time_delta = time - tick_time;
	time = tick_time; // use synced time from game tick
	
	static bool is_first_tick = true;
	update_base_camps();
	update_base_camp_pals();
	for (auto& [pal, pal_man] : pal_time_window_mans)
	{
		pal_man.on_tick(time, time_delta);
	}
	if (is_first_tick) {
		is_first_tick = false;
		PalInternalUtility::GetInstance()->SendChat(std::wstring(L"awm looks good"));
	}
}

void AbnormalWorkerMonitor::init_time_windows_and_cooldown()
{
	cooldown = 5.0;
	time_windows.push_back(TimeWindow("IsSanityLow", 60.0, 6, 10.0, IsSanityLow)); // sanity
	time_windows.push_back(TimeWindow("IsHungry", 60.0, 6, 10.0, IsHungry)); // hunger
	time_windows.push_back(TimeWindow("IsOutSide", 60.0, 30, 1.0, IsOutside)); // outside
	time_windows.push_back(TimeWindow("IsLocHigh", 60.0, 6, 10.0, IsLocHigh)); // high
	time_windows.push_back(TimeWindow("IsLocLow", 60.0, 6, 10.0, isLocLow)); // low
	time_windows.push_back(TimeWindow("IsSanityLow", 60.0, 6, 10.0, IsSanityLow)); // sanity
	
	//cooldown = 30.0;
	//// simply set threshold to 1, so once it happens, it will emit
	//time_windows.push_back(TimeWindow("IsSanityLow", .0, 1, .0, IsSanityLow)); // sanity
	//time_windows.push_back(TimeWindow("IsHungry", .0, 1, .0, IsHungry)); // hunger
	//time_windows.push_back(TimeWindow("IsOutSide", .0, 1, .0, IsOutside)); // outside
	//time_windows.push_back(TimeWindow("IsLocHigh", .0, 1, .0, IsLocHigh)); // high
	//time_windows.push_back(TimeWindow("IsLocLow", .0, 1, .0, isLocLow)); // low
	//time_windows.push_back(TimeWindow("IsSanityLow", .0, 1, .0, IsSanityLow)); // sanity
}

void AbnormalWorkerMonitor::init_check_spawn_point()
{
	update_base_camps();
	update_base_camp_pals();
	// iterate palset, for each pal get base camp id (and location), get spawn point location, compare if  the latter.Z is higher
	for (auto& pal : pal_set)
	{
		UPalUtility* PalUtility = PalInternalUtility::GetInstance()->GetPalUtility();
		UPalIndividualCharacterParameter* PalIndividualCharacterParameter = PalUtility->GetIndividualCharacterParameterByActor(pal);
		// do NOT use ->BaseCampId
		FGuid BaseCampId = PalIndividualCharacterParameter->GetBaseCampId();
		FVector BaseCampLocation;
		BaseCampLocation = base_camp_id_2_location[BaseCampId];
		FVector SpawnLocation = pal->SpawnLocation_ForServer;
		if (SpawnLocation.Z < BaseCampLocation.Z)
		{
			FVector Location = BaseCampLocation;
			Location.Z = BaseCampLocation.Z + 200;
			FQuat Rotation = base_camp_id_2_rotation[BaseCampId];
			PalUtility->TeleportAroundLoccation(pal, Location, Rotation);
			char buffer[1024];
			std::snprintf(buffer, sizeof(buffer), "teleport %s of spawn point (%f %f %f) to base camp\n",
				pal->GetName().c_str(),
				SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
			g_Console->printdbg(buffer, Console::Colors::green);
			Logger::GetInstance().AddLog(buffer);
		}
	}
}

void AbnormalWorkerMonitor::update_base_camps()
{
	// get current base camp ids
	TArray<FGuid> BaseCampIds;
	PalInternalUtility::GetInstance()->GetPalBaseCampManager()->GetBaseCampIds(&BaseCampIds);
	std::set<FGuid, FGuidLess> base_camp_id_set_temp;
	for (int i = 0; i < BaseCampIds.Count(); i++)
	{
		base_camp_id_set_temp.insert(BaseCampIds[i]);
	}
	// compare two sets, remove outdated id, add new id
	for (auto id : base_camp_id_set)
	{
		if (base_camp_id_set_temp.find(id) == base_camp_id_set_temp.end())
		{
			base_camp_id_set.erase(id);
			base_camp_id_2_location_point.erase(id);
			base_camp_id_2_location.erase(id);
			base_camp_id_2_rotation.erase(id);
			g_Console->printdbg("erase base camp id %s\n", Console::Colors::blue, PalInternalUtility::GetInstance()->FGuid2String(id).c_str());
		}
	}
	bool has_new_base_camp = false;
	for (auto id : base_camp_id_set_temp)
	{
		if (base_camp_id_set.find(id) == base_camp_id_set.end())
		{
			has_new_base_camp = true;
			break;
		}
	}
	if (has_new_base_camp)
	{
		// TODO: use LocationMap when TMap is available, so no need to use the uobject scanning that takes more than 1s
		// and then it s fine to place awm tick in game tick thread (i think) instead of a new thread to sync with game tick
		// the thread and sync acheives following goals:
		// thread: avoid blocking game tick
		// sync: no run awm tick when out of game (no game tick)
		// TMap<FGuid, UPalLocationBase*> LocationMap;
		auto start = std::chrono::high_resolution_clock::now(); // debug
		for (int idx = 0; idx < UObject::GObjects->Num(); idx++)
		// for (int idx = 0; idx < LocationMap.c; idx++)
		{
			// about 200ms
			if (idx % 100000 == 0)
			{
				auto end = std::chrono::high_resolution_clock::now(); // debug
				auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
				int ms = duration.count();
				if (ms) {
					g_Console->printdbg("update base camp cost %d ms for first %d objects\n", Console::Colors::blue, ms, idx);
				}
				start = std::chrono::high_resolution_clock::now(); // debug
			}
			UObject* obj = UObject::GObjects->GetByIndex(idx);
			// UObject* obj = UObject::GObjects->GetByIndex(idx);
			if (obj && obj->GetFullName().find("PalLocationPointBaseCamp") != std::string::npos
				&& obj->IsA(UPalLocationPointBaseCamp::StaticClass()) && !obj->IsDefaultObject())
			{
				UPalLocationPointBaseCamp* location_point = static_cast<UPalLocationPointBaseCamp*>(obj);
				FGuid id = location_point->GetBaseCampId();
				if (base_camp_id_set.find(id) != base_camp_id_set.end() || base_camp_id_set_temp.find(id) == base_camp_id_set_temp.end())
				{
					continue;
				}
				base_camp_id_2_location_point[id] = location_point;
				base_camp_id_2_location[id] = location_point->GetLocation();
				base_camp_id_2_rotation[id] = location_point->GetRotation();
				// print prev sets
				for (auto& id : base_camp_id_set)
				{
					g_Console->printdbg("prev base camp id %s at (%f %f %f) of (%f %f %f %f)\n",
						Console::Colors::blue,
						PalInternalUtility::GetInstance()->FGuid2String(id).c_str(),
						base_camp_id_2_location[id].X, base_camp_id_2_location[id].Y, base_camp_id_2_location[id].Z,
						base_camp_id_2_rotation[id].X, base_camp_id_2_rotation[id].Y, base_camp_id_2_rotation[id].Z, base_camp_id_2_rotation[id].W);
				}
				g_Console->printdbg("add base camp id %s at (%f %f %f) of (%f %f %f %f)\n",
					Console::Colors::blue,
					PalInternalUtility::GetInstance()->FGuid2String(id).c_str(),
					base_camp_id_2_location[id].X, base_camp_id_2_location[id].Y, base_camp_id_2_location[id].Z,
					base_camp_id_2_rotation[id].X, base_camp_id_2_rotation[id].Y, base_camp_id_2_rotation[id].Z, base_camp_id_2_rotation[id].W);
				base_camp_id_set.insert(id);
			}
		}
	}
}

void AbnormalWorkerMonitor::update_base_camp_pals()
{
	auto start = std::chrono::high_resolution_clock::now(); // debug
	PalSet pal_set_temp;
	UPalBaseCampManager* PalBaseCampManager = PalInternalUtility::GetInstance()->GetPalBaseCampManager();
	// iterate base camps, gen pal set
	for (auto& id : base_camp_id_set)
	{
		FGuid BaseCampId = id;
		UPalBaseCampModel* PalBaseCampModel;
		PalBaseCampManager->TryGetModel(BaseCampId, &PalBaseCampModel);
		TArray<class UPalIndividualCharacterSlot*> PalIndividualCharacterSlots;
		PalBaseCampModel->WorkerDirector->GetCharacterHandleSlots(&PalIndividualCharacterSlots);
		for (int j = 0; j < PalIndividualCharacterSlots.Count(); j++)
		{
			UPalIndividualCharacterSlot* PalIndividualCharacterSlot = PalIndividualCharacterSlots[j];
			UPalIndividualCharacterHandle* PalIndividualCharacterHandle = PalIndividualCharacterSlot->GetHandle();
			if (!PalIndividualCharacterHandle)
				continue;
			APalCharacter* PalCharacter = PalIndividualCharacterHandle->TryGetIndividualActor();
			pal_set_temp.insert(PalCharacter);
		}
	}
	// compare two sets, remove outdated pal, add new pal
	for (auto it = pal_set.begin(); it != pal_set.end();)
	{
		if (pal_set_temp.find(*it) == pal_set_temp.end())
		{
			pal_time_window_mans.erase(*it);
			it = pal_set.erase(it);
			// FIXME: "erase pal None", seems fine, just a note
			g_Console->printdbg("erase pal %s\n", Console::Colors::blue, (*it)->GetName().c_str());
		}
		else
		{
			++it;
		}
	}
	for (auto& pal : pal_set_temp)
	{
		if (pal_set.find(pal) == pal_set.end())
		{
			UPalUtility* PalUtility = PalInternalUtility::GetInstance()->GetPalUtility();
			UPalIndividualCharacterParameter* PalIndividualCharacterParameter = PalUtility->GetIndividualCharacterParameterByActor(pal);
			if (!PalIndividualCharacterParameter)
			{
				return;
			}
			FGuid BaseCampId = PalIndividualCharacterParameter->GetBaseCampId();
			FVector BaseCampLocation = base_camp_id_2_location[BaseCampId];
			FVector SpawnLocation = pal->SpawnLocation_ForServer;
			FVector Location = pal->K2_GetActorLocation();

			pal_set.insert(pal);
			pal_time_window_mans[pal] = SingleAbnormalWorkerMonitor(pal, time_windows, cooldown);

			g_Console->printdbg("add pal %s at (%f %f %f) spawn at (%f %f %f)\n", 
				Console::Colors::blue, 
				pal->GetName().c_str()
				, Location.X, Location.Y, Location.Z
				, SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
		}
	}
	pal_set = pal_set_temp;
	auto end = std::chrono::high_resolution_clock::now(); // debug
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	int ms = duration.count();
	if (ms) {
		g_Console->printdbg("update base camp pals cost %d ms\n", Console::Colors::blue, ms);
	}
}
