// Copyright 2018-2021 Andreas Jung
// SPDX-License-Identifier: GPL-3.0-only

#include "F12020UdpClrMapper.h"

namespace adjsw::F12020
{
   F12020UdpClrMapper::F12020UdpClrMapper()
   {
      m_parser = new F12020ElementaryParser();
      arr = gcnew array<Byte>(4096);
      len = 0;
      pUnmanaged = Marshal::AllocHGlobal(512 * 1024);

      Drivers = gcnew array<DriverData^>(22);

      for (int i = 0; i < Drivers->Length; ++i)
         Drivers[i] = gcnew DriverData();

      SessionInfo = gcnew adjsw::F12020::SessionInfo();
      EventList = gcnew SessionEventList();
   }

   F12020UdpClrMapper::~F12020UdpClrMapper()
   {
      delete m_parser;
      Marshal::FreeHGlobal(pUnmanaged);
   }

   void F12020UdpClrMapper::m_UpdateTelemetry(int i)
   {
      DriverData^ driver = Drivers[i];
      if (!driver->Present)
         return;

      driver->WearDetail->TempFrontLeftInner = m_parser->telemetry.m_carTelemetryData[i].m_tyresInnerTemperature[2];
      driver->WearDetail->TempFrontRightInner = m_parser->telemetry.m_carTelemetryData[i].m_tyresInnerTemperature[3];
      driver->WearDetail->TempRearLeftInner = m_parser->telemetry.m_carTelemetryData[i].m_tyresInnerTemperature[0];
      driver->WearDetail->TempRearRightInner = m_parser->telemetry.m_carTelemetryData[i].m_tyresInnerTemperature[1];

      driver->WearDetail->TempFrontLeftOuter = m_parser->telemetry.m_carTelemetryData[i].m_tyresSurfaceTemperature[2];
      driver->WearDetail->TempFrontRightOuter = m_parser->telemetry.m_carTelemetryData[i].m_tyresSurfaceTemperature[3];
      driver->WearDetail->TempRearLeftOuter = m_parser->telemetry.m_carTelemetryData[i].m_tyresSurfaceTemperature[0];
      driver->WearDetail->TempRearRightOuter = m_parser->telemetry.m_carTelemetryData[i].m_tyresSurfaceTemperature[1];

      driver->WearDetail->TempBrakeFrontLeft = m_parser->telemetry.m_carTelemetryData[i].m_brakesTemperature[2];
      driver->WearDetail->TempBrakeFrontRight = m_parser->telemetry.m_carTelemetryData[i].m_brakesTemperature[3];
      driver->WearDetail->TempBrakeRearLeft = m_parser->telemetry.m_carTelemetryData[i].m_brakesTemperature[0];
      driver->WearDetail->TempBrakeRearRight = m_parser->telemetry.m_carTelemetryData[i].m_brakesTemperature[1];

      driver->WearDetail->TempEngine = m_parser->telemetry.m_carTelemetryData[i].m_engineTemperature;
   }

   bool F12020UdpClrMapper::Proceed(array<System::Byte>^ input)
   {

      arr = input;
      len = input->Length;

      if (len > (512*1024))
         return false;

      Marshal::Copy(arr, 0, pUnmanaged, len);
      auto p = reinterpret_cast<const uint8_t*>(pUnmanaged.ToPointer());

      while (len)
      {
         unsigned processed = m_parser->ProceedPacket(p, len);
         len -= processed;
         p += processed;
         m_Update();
      }
      return true;
   }

   void F12020UdpClrMapper::InsertTestData()
   {
      m_Clear();

      std::mt19937 engine;

      constexpr unsigned CNT_SIMDATA = 20;
      constexpr unsigned PLAYER_IDX = 0;
      constexpr unsigned LAPS = 4;
      assert(CNT_SIMDATA <= Drivers->Length);
      
      SessionInfo->Session = SessionType::Race;
      SessionInfo->SessionFinshed = false;
      SessionInfo->TotalLaps = 10;
      SessionInfo->CurrentLap = 5;
      SessionInfo->EventTrack = Track::Austria;

      CountDrivers = CNT_SIMDATA;

      // insert names
      for (unsigned i = 0; i < CNT_SIMDATA ; ++i)
      {
         DriverData^ driver = Drivers[i];
         driver->Name = "Dummy Data " + (i + 1);
         driver->Present = true;
         driver->VisualTyre = F1VisualTyre::Soft;

         if (i == 2)
            driver->VisualTyre = F1VisualTyre::Medium;

         if (i == 3)
            driver->VisualTyre = F1VisualTyre::Hard;

         if (i == 4)
            driver->VisualTyre = F1VisualTyre::Intermediate;

         if (i == 5)
            driver->VisualTyre = F1VisualTyre::Wet;

         if (i == 6)
            driver->VisualTyres->Add(F1VisualTyre::Medium);

         driver->VisualTyres->Add(driver->VisualTyre);
         driver->VisualTyres = driver->VisualTyres;
      }
      Drivers[PLAYER_IDX]->Name = "Player";
      Drivers[PLAYER_IDX]->IsPlayer=true;

      // insert laptimes
      std::normal_distribution<float> dist(33.f, 2.f);
      for (unsigned i = 0; i < CNT_SIMDATA; ++i)
      {
         DriverData^ driver = Drivers[i];

         for (unsigned j = 0; j < LAPS; ++j)
         {
            driver->Laps[j]->Sector1 = dist(engine);
            driver->Laps[j]->Sector2 = dist(engine);
            driver->Laps[j]->Lap = driver->Laps[j]->Sector1 + driver->Laps[j]->Sector2 + dist(engine);
         }
         driver->LapNr = LAPS;
         driver->Status = DriverStatus::OnTrack;
      }

      // update accumulated laptimes
      for (unsigned j = 0; j < CNT_SIMDATA; ++j)
      {
         DriverData^ driver = Drivers[j];
         float driverTimeAfterLap = 0.f;
         for (unsigned i = 0; i < LAPS; ++i)
         {
            driverTimeAfterLap += Drivers[j]->Laps[i]->Lap;
            Drivers[j]->Laps[i]->LapsAccumulated = driverTimeAfterLap;
         }
      }

      // update delta to player
      {
         float playerTimeAfterLap = Drivers[PLAYER_IDX]->Laps[LAPS-1]->LapsAccumulated;
         float playerTimeBeforeLastSector = 
            playerTimeAfterLap 
            - Drivers[PLAYER_IDX]->Laps[LAPS - 1]->Lap 
            + Drivers[PLAYER_IDX]->Laps[LAPS - 1]->Sector1
            + Drivers[PLAYER_IDX]->Laps[LAPS - 1]->Sector2;

         for (unsigned i = 0; i < CNT_SIMDATA; ++i)
         {
            DriverData^ driver = Drivers[i];
            driver->TimedeltaToPlayer = driver->Laps[LAPS - 1]->LapsAccumulated - playerTimeAfterLap;

            float driverTimeBeforeLastSector =
               driver->Laps[LAPS - 1]->LapsAccumulated
               - driver->Laps[LAPS - 1]->Lap
               + driver->Laps[LAPS - 1]->Sector1
               + driver->Laps[LAPS - 1]->Sector2;

            driver->LastTimedeltaToPlayer = driverTimeBeforeLastSector - playerTimeBeforeLastSector;
         }

         for (unsigned i = 0; i < CNT_SIMDATA; ++i)
         {
            DriverData^ driver = Drivers[i];
            driver->TimedeltaToPlayer = driver->Laps[LAPS - 1]->LapsAccumulated - playerTimeAfterLap;

            float driverTimeBeforeLastSector =
               driver->Laps[LAPS - 1]->LapsAccumulated
               - driver->Laps[LAPS - 1]->Lap
               + driver->Laps[LAPS - 1]->Sector1
               + driver->Laps[LAPS - 1]->Sector2;

            driver->LastTimedeltaToPlayer = driverTimeBeforeLastSector - playerTimeBeforeLastSector;
         }
      }

      // update positions
      array<DriverData^>^ driversSort = gcnew array<DriverData^>(CNT_SIMDATA);
      for (unsigned i = 0; i < CNT_SIMDATA; ++i)
      {
         driversSort[i] = Drivers[i];
      }

      for (unsigned j = 0; j < CNT_SIMDATA; ++j)
      {
         float bestTime = 999999.f;
         unsigned bestIdx = 0;
         for (unsigned i = 0; i < CNT_SIMDATA; ++i)
         {
            if ((driversSort[i] != nullptr) && driversSort[i]->Laps[LAPS - 1]->LapsAccumulated < bestTime)
            {
               bestIdx = i;
               bestTime = driversSort[i]->Laps[LAPS - 1]->LapsAccumulated;
            }
         }
         driversSort[bestIdx]->Pos = j + 1;
         driversSort[bestIdx] = nullptr;
      }

      // update car status
      Drivers[PLAYER_IDX]->WearDetail->WearFrontLeft = 39;
      Drivers[PLAYER_IDX]->WearDetail->WearFrontRight = 12;
      Drivers[PLAYER_IDX]->WearDetail->WearRearLeft = 88;
      Drivers[PLAYER_IDX]->WearDetail->WearRearRight = 19;
      Drivers[PLAYER_IDX]->WearDetail->DamageFrontLeft = 35;
      Drivers[PLAYER_IDX]->WearDetail->TempFrontLeftOuter = 130;
      Drivers[PLAYER_IDX]->WearDetail->TempFrontLeftInner = 95;
      Drivers[PLAYER_IDX]->WearDetail->TempFrontRightOuter = 100;
      Drivers[PLAYER_IDX]->WearDetail->TempFrontRightInner = 77;
   }

   void F12020UdpClrMapper::m_Clear()
   {
      SessionInfo->SessionFinshed = false;
      SessionInfo->CurrentLap = 1;
      EventList->Events->Clear();
      EventList = EventList; // force NPC
      CountDrivers = 0;
      Classification = nullptr;
      m_parser->classification.m_numCars = 0;

      for each (DriverData^ dat in Drivers)
      {
         dat->Reset();
      }
   }


   void F12020UdpClrMapper::m_Update()
   {
      m_UpdateEvent();
      m_UpdateDrivers();
      m_UpdateClassification();
   }

   void F12020UdpClrMapper::m_UpdateEvent()
   {
      if (m_parser->event.m_eventStringCode[0] != 0)
      {
         //new event
         if (!strncmp((const char*)m_parser->event.m_eventStringCode, "SSTA", 4))
         {
            m_Clear();
            SessionEvent^ e = gcnew SessionEvent();
            e->TimeCode = DateTime::Now;
            e->Type = EventType::SessionStarted;
            e->CarIndex = 0; // N/A
            EventList->Events->Add(e);

         }
         else if (!strncmp((const char*)m_parser->event.m_eventStringCode, "SEND", 4))
         {
            SessionEvent^ e = gcnew SessionEvent();
            e->TimeCode = DateTime::Now;
            e->Type = EventType::SessionEnded;
            e->CarIndex = 0; // N/A
            EventList->Events->Add(e);
            SessionInfo->SessionFinshed = true;
         }
         else if (!strncmp((const char*)m_parser->event.m_eventStringCode, "FTLP", 4))
         {
            SessionEvent^ e = gcnew SessionEvent();
            e->TimeCode = DateTime::Now;
            e->Type = EventType::FastestLap;
            e->CarIndex = m_parser->event.m_eventDetails.FastestLap.vehicleIdx;
            // TODO add parameters
            EventList->Events->Add(e);
         }
         else if (!strncmp((const char*)m_parser->event.m_eventStringCode, "RTMT", 4))
         {
            SessionEvent^ e = gcnew SessionEvent();
            e->TimeCode = DateTime::Now;
            e->Type = EventType::Retirement;
            e->CarIndex = m_parser->event.m_eventDetails.Retirement.vehicleIdx;
            EventList->Events->Add(e);
         }
         else if (!strncmp((const char*)m_parser->event.m_eventStringCode, "DRSE", 4))
         {
            SessionEvent^ e = gcnew SessionEvent();
            e->TimeCode = DateTime::Now;
            e->Type = EventType::DRSenabled;
            e->CarIndex = 0; // N/A
            EventList->Events->Add(e);
         }
         else if (!strncmp((const char*)m_parser->event.m_eventStringCode, "DRSD", 4))
         {
            SessionEvent^ e = gcnew SessionEvent();
            e->TimeCode = DateTime::Now;
            e->Type = EventType::DRSdisabled;
            e->CarIndex = 0; // N/A
            EventList->Events->Add(e);
         }

         else if (!strncmp((const char*)m_parser->event.m_eventStringCode, "TMPT", 4))
         {
            SessionEvent^ e = gcnew SessionEvent();
            e->TimeCode = DateTime::Now;
            e->Type = EventType::TeamMateInPits;
            e->CarIndex = m_parser->event.m_eventDetails.TeamMateInPits.vehicleIdx;
            EventList->Events->Add(e);
         }

         else if (!strncmp((const char*)m_parser->event.m_eventStringCode, "CHQF", 4))
         {
            SessionEvent^ e = gcnew SessionEvent();
            e->TimeCode = DateTime::Now;
            e->Type = EventType::ChequeredFlag;
            e->CarIndex = 0; // N/A
            EventList->Events->Add(e);
         }
         else if (!strncmp((const char*)m_parser->event.m_eventStringCode, "RCWN", 4))
         {
            SessionEvent^ e = gcnew SessionEvent();
            e->TimeCode = DateTime::Now;
            e->Type = EventType::RaceWinner;
            e->CarIndex = m_parser->event.m_eventDetails.RaceWinner.vehicleIdx;
            EventList->Events->Add(e);
         }
         else if (!strncmp((const char*)m_parser->event.m_eventStringCode, "PENA", 4))
         {
            SessionEvent^ e = gcnew SessionEvent();
            e->TimeCode = DateTime::Now;
            e->Type = EventType::PenaltyIssued;
            e->PenaltyType = PenaltyTypes(m_parser->event.m_eventDetails.Penalty.penaltyType);
            e->LapNum = m_parser->event.m_eventDetails.Penalty.lapNum;
            e->CarIndex = m_parser->event.m_eventDetails.Penalty.vehicleIdx;
            e->OtherVehicleIdx = m_parser->event.m_eventDetails.Penalty.otherVehicleIdx;
            e->InfringementType = InfringementTypes(m_parser->event.m_eventDetails.Penalty.infringementType);            
            e->TimeGained = m_parser->event.m_eventDetails.Penalty.time;
            e->PlacesGained = m_parser->event.m_eventDetails.Penalty.placesGained;
            e->PenaltyServed = false;
            EventList->Events->Add(e);

            if (e->LapNum < Drivers[e->CarIndex]->Laps->Length)
            {
               int lapIdx = e->LapNum - 1;
               if (lapIdx < 0)
                  lapIdx = 0;

               Drivers[e->CarIndex]->Laps[lapIdx]->Incidents->Add(e);
            }
               

            switch (e->PenaltyType)
            {
               case PenaltyTypes::DriveThrough:
               case PenaltyTypes::StopGo:
               case PenaltyTypes::Disqualified:
               case PenaltyTypes::Retired:
                  Drivers[e->CarIndex]->PitPenalties->Add(e);
                  Drivers[e->CarIndex]->NPC("PitPenalties");
                  break;
            }


         }
         else if (!strncmp((const char*)m_parser->event.m_eventStringCode, "SPTP", 4))
         {
            SessionEvent^ e = gcnew SessionEvent();
            e->TimeCode = DateTime::Now;
            e->Type = EventType::SpeedTrapTriggered;
            e->CarIndex = m_parser->event.m_eventDetails.SpeedTrap.vehicleIdx;
            // TODO add parameters
            EventList->Events->Add(e);
         }
      }

      m_parser->event.m_eventStringCode[0] = 0; // inhibit another parse of the same event
   }

   void F12020UdpClrMapper::m_UpdateDrivers()
   {
      // prevent left players to disappear in list
      // which means during a session, the maximum number of players/ai ever present are shown.
      if (m_parser->participants.m_numActiveCars > CountDrivers)
         CountDrivers = m_parser->participants.m_numActiveCars;

      // Update Session
      SessionInfo->EventTrack = Track(m_parser->session.m_trackId);
      SessionInfo->Session = SessionType(m_parser->session.m_sessionType);
      SessionInfo->RemainingTime = m_parser->session.m_sessionTimeLeft;
      SessionInfo->TotalLaps = m_parser->session.m_totalLaps;

      // Lapdata + Name
      for (int i = 0; i < Drivers->Length; ++i)
      {
         if (String::IsNullOrEmpty(Drivers[i]->TelemetryName) && m_parser->participants.m_participants[i].m_raceNumber)
         {
            m_UpdateDriverName(i);
         }

         auto& lapNative = m_parser->lap.m_lapData[i];
         auto lapClr = Drivers[i]->Laps;

         Drivers[i]->Pos = lapNative.m_carPosition;

         unsigned lap_num = 0;
         if (Drivers[i]->LapNr != lapNative.m_currentLapNum) // Update last laptime
         {
            if (lapNative.m_currentLapNum > lap_num)
               lap_num = lapNative.m_currentLapNum;

            Drivers[i]->LapNr = lapNative.m_currentLapNum;
            Drivers[i]->TyreAge = Drivers[i]->LapNr - Drivers[i]->m_lapTiresFitted;
            if (Drivers[i]->LapNr > 0) // should always be true
            {
               lapClr[Drivers[i]->LapNr - 1]->Sector1 = 0;
               lapClr[Drivers[i]->LapNr - 1]->Sector2 = 0;
               lapClr[Drivers[i]->LapNr - 1]->Lap = 0;
            }
            if (Drivers[i]->LapNr > 1)
            {
               lapClr[Drivers[i]->LapNr - 2]->Lap = lapNative.m_lastLapTime;

               if (Drivers[i]->LapNr == 2)
                  lapClr[0]->LapsAccumulated = lapClr[0]->Lap;
               else
               {
                  lapClr[Drivers[i]->LapNr - 2]->LapsAccumulated = lapClr[Drivers[i]->LapNr - 2]->Lap + lapClr[Drivers[i]->LapNr - 3]->LapsAccumulated;
               }

            }
         }

         else if (Drivers[i]->LapNr > 0) // Update Sector1+2 if available
         {
            auto currentLap = lapClr[Drivers[i]->LapNr - 1];
            if (currentLap->Sector1 == 0)
            {
               if (lapNative.m_sector > 0)
                  currentLap->Sector1 = lapNative.m_sector1TimeInMS / 1000.0f;
            }

            if (currentLap->Sector2 == 0)
            {
               if (lapNative.m_sector > 1)
                  currentLap->Sector2 = lapNative.m_sector2TimeInMS / 1000.0f;
            }
         }

         if (lap_num > SessionInfo->CurrentLap)
         {
            SessionInfo->CurrentLap = std::min<int>(lap_num, SessionInfo->TotalLaps); // clamp to TotalLaps to prevent the post race lap to count behind maximum
         }
            
      }

      for (int i = 0; i < CountDrivers; ++i)
      {
         switch (m_parser->lap.m_lapData[i].m_resultStatus)
         {
            // According to doc:
            // Result status - 0 = invalid, 1 = inactive, 2 = active
            // 3 = finished, 4 = disqualified, 5 = not classified
            // 6 = retired
            // BUT: 7 seems to be a legit code for a dnf car!
         case 2:
         case 3:
            Drivers[i]->Present = true;
            break;
         default:
            Drivers[i]->Present = false;
            Drivers[i]->TimedeltaToPlayer = 0; // triggers UI Update
            break;
         }
      }

      DriverData^ player = nullptr;

      if (m_parser->lap.m_header.m_playerCarIndex < Drivers->Length) // in visitor modes index is 255
         player = Drivers[m_parser->lap.m_header.m_playerCarIndex]; 

      DriverData^ leader = nullptr;

      bool qualyfiyingDelta = false; // (for training or Q1-Q3 use bestlap delta)
      
      switch (SessionInfo->Session)
      {
      case SessionType::P1:
      case SessionType::P2:
      case SessionType::P3:
      case SessionType::ShortPractice:
      case SessionType::Q1:
      case SessionType::Q2:
      case SessionType::Q3:
      case SessionType::ShortQ:
         qualyfiyingDelta = true;
         break;
      default: break;
      }
      

      // find leader (if available)
      for (int i = 0; i < Drivers->Length; ++i)
      {
         DriverData^ car = Drivers[i];
         if (
            //car->Present && // not required, the in qualy the car might be retired after setting lap which is still valid
            (car->Pos == 1))
         {
            leader = car;
            leader->TimedeltaToLeader = 0;
            break;
         }
      }

      // m_parser->lap.m_header.m_playerCarIndex defaults to 0 and might change when the first actual packet arrives
      // which means we must check, if we declared first car 0 by accident as player and revert in that case!
      if (m_parser->lap.m_header.m_playerCarIndex != 0)
         Drivers[0]->IsPlayer = false;

      if (player)
      {
         player->IsPlayer = true;
         player->TimedeltaToPlayer = 0;

         if (!player->LapNr)
            return;
      }

      // update the delta Time, tyre and car damage
      for (int i = 0; i < Drivers->Length; ++i)
      {
         DriverData^ car = Drivers[i];
         if (!car->Present)
            continue;

         // delta to player
         if (player)
         {
            if (!car->IsPlayer)
               qualyfiyingDelta ? m_UpdateTimeDeltaQualy(player, i, true) : m_UpdateTimeDeltaRace(player, i, true);
         }
         else
         {
            car->LastTimedeltaToPlayer = 0;
            car->TimedeltaToPlayer = 0;
         }

         // delta to leader
         if (leader && (car != leader) )
            qualyfiyingDelta ? m_UpdateTimeDeltaQualy(leader, i, false) : m_UpdateTimeDeltaRace(leader, i, false);

         m_UpdateTelemetry(i);
         m_UpdateTyre(i);
         m_UpdateDamage(i);

         if (car->FastestLap->Lap != m_parser->lap.m_lapData[i].m_bestLapTime)
         {
            car->FastestLap->Lap = m_parser->lap.m_lapData[i].m_bestLapTime;
            car->NPC("FastestLap");
         }

         car->PenaltySeconds = m_parser->lap.m_lapData[i].m_penalties;
         car->Tyre = F1Tyre(m_parser->status.m_carStatusData[i].m_actualTyreCompound);
         car->VisualTyre = F1VisualTyre(m_parser->status.m_carStatusData[i].m_visualTyreCompound);
         if (!car->VisualTyres->Count && (static_cast<int>(car->VisualTyre) != 0))
         {
            // add the first tyre at the start of race
            car->VisualTyres->Add(car->VisualTyre);
            car->VisualTyres = car->VisualTyres; // trigger NotifyPorpertyChanged
         }

         // car->TyreAge = m_parser->status.m_carStatusData[i].m_tyresAgeLaps; -> NOOO its a lie! (Game telemetry has invalid data).

         DriverStatus oldDriverStatus = car->Status;

         switch (m_parser->lap.m_lapData[i].m_resultStatus)
         {
            //0 = invalid, 1 = inactive, 2 = active
            // 3 = finished, 4 = disqualified, 5 = not classified
            // 6 = retired - apparently 7 also = retired
         case 4:
            car->Status = DriverStatus::DSQ;
            break;

         case 5:
         case 6:
         case 7:
            car->Status = DriverStatus::DNF;
            break;

         default:
            switch (m_parser->lap.m_lapData[i].m_pitStatus)
            {
            case 1: car->Status = DriverStatus::Pitlane; break;
            case 2: car->Status = DriverStatus::Pitting; car->m_hasPitted = true; break;

            default:
               switch (m_parser->lap.m_lapData[i].m_driverStatus)
               {
                  // Status of driver - 0 = in garage, 1 = flying lap
                  // 2 = in lap, 3 = out lap, 4 = on track
               //case 0: car->Status = DriverStatus::Garage; break;
               case 0:
                  car->Status = DriverStatus::Garage;
                  break;

               case 1:
               case 2:
               case 3:
               case 4:
                  car->Status = DriverStatus::OnTrack; break;
               default:
                  car->Status = DriverStatus::Garage; // just assume....
                  break;
               }
               break;
            }
            break;
         }

         if (oldDriverStatus == DriverStatus::Pitting && car->Status != oldDriverStatus)
         {
            // deduce the tyres were probably be changed (we don´t get specific notification about that)
            car->VisualTyres->Add(car->VisualTyre);
            car->VisualTyres = car->VisualTyres; // trigger NotifyPorpertyChanged
         }

         if (oldDriverStatus == DriverStatus::Pitlane && (car->Status == DriverStatus::OnTrack))
         {
            if (!car->m_hasPitted)
            {
               // in pits without pitstop -> probably served drive through penalty
               for each (SessionEvent^  penalty in car->PitPenalties)
               {
                  if ((penalty->PenaltyType == PenaltyTypes::DriveThrough) && (!penalty->PenaltyServed))
                  {
                     penalty->PenaltyServed = true;
                     car->NPC("PitPenalties");
                     break;
                  }
               }
            }
            else
            {
               // car has pitted
               car->m_lapTiresFitted = car->LapNr;
               car->TyreAge = 0;
               
               // also see if a penalty was served:
               for each (SessionEvent ^ penalty in car->PitPenalties)
               {
                  if ((penalty->PenaltyType != PenaltyTypes::DriveThrough) && (!penalty->PenaltyServed))
                  {
                     if (penalty->InfringementType == InfringementTypes::PitLaneSpeeding)
                     {
                        // pit lane speeding can´t be serverd immediately, check if it is old enough
                        if ((DateTime::Now - penalty->TimeCode).TotalSeconds > 60)
                        {
                           penalty->PenaltyServed = true;
                           car->NPC("PitPenalties");
                           break;
                        }
                     }
                     else
                     {
                        if (!penalty->PenaltyServed)
                        {
                           penalty->PenaltyServed = true;
                           car->NPC("PitPenalties");
                           break;
                        }
                     }                     
                  }
               }
            }
            car->m_hasPitted = false;
         }


         if (m_parser->participants.m_participants[i].m_teamId < 10)
            car->Team = F1Team(m_parser->participants.m_participants[i].m_teamId);

         else
            car->Team = F1Team::Classic;
      }
   }

   void F12020UdpClrMapper::m_UpdateTimeDeltaRace(DriverData^ reference, int i, bool toPlayer)
   {
      DriverData^ opponent = Drivers[i];
      if (!opponent->Present)
         return;

      // player passed Sector 2:
      bool found = false;
      int lapIdx = reference->LapNr - 1;
      unsigned lapSector = 2;

      // search the greatest Sector time both cars have
      while (!found)
      {
         if ((opponent->LapNr - 1) < lapIdx)
         {
            --lapIdx;
            lapSector = 2;
            continue;
         }

         switch (lapSector)
         {
         case 0:
            if ((reference->Laps[lapIdx]->Sector1) && opponent->Laps[lapIdx]->Sector1)
            {
               found = true;
            }
            break;

         case 1:
            if ((reference->Laps[lapIdx]->Sector2) && opponent->Laps[lapIdx]->Sector2)
            {
               found = true;
            }
            break;

         case 2:
            if ((reference->Laps[lapIdx]->Lap) && opponent->Laps[lapIdx]->Lap)
            {
               found = true;
            }
            break;
         }

         if (!found)
         {
            if ((lapIdx == 0) && (lapSector == 0))
               break;

            if (lapSector)
               --lapSector;
            else
            {
               --lapIdx;
               lapSector = 2;
            }
            continue;
         }

         float timePlayer = 0;
         float timeOpponent = 0;

         if (found)
         {
            if (lapIdx > 0)
            {
               timePlayer = reference->Laps[lapIdx - 1]->LapsAccumulated;
               timeOpponent = opponent->Laps[lapIdx - 1]->LapsAccumulated;
            }

            switch (lapSector)
            {
            case 0:
               timePlayer += reference->Laps[lapIdx]->Sector1;
               timeOpponent += opponent->Laps[lapIdx]->Sector1;
               break;

            case 1:
               timePlayer += reference->Laps[lapIdx]->Sector1 + reference->Laps[lapIdx]->Sector2;
               timeOpponent += opponent->Laps[lapIdx]->Sector1 + opponent->Laps[lapIdx]->Sector2;
               break;

            case 2:
               timePlayer += reference->Laps[lapIdx]->Lap;
               timeOpponent += opponent->Laps[lapIdx]->Lap;
               break;
            }
         }

         auto newDelta = timePlayer - timeOpponent;
         if (toPlayer)
         {
            // take penalties into consideration
            timePlayer += reference->PenaltySeconds;
            newDelta -= m_parser->lap.m_lapData[i].m_penalties;

            if (newDelta != opponent->TimedeltaToPlayer)
            {
               opponent->LastTimedeltaToPlayer = opponent->TimedeltaToPlayer;
               opponent->TimedeltaToPlayer = newDelta;
            }
         }
         else
         {
            newDelta *= -1;
            if (newDelta != opponent->TimedeltaToLeader)
            {
               opponent->TimedeltaToLeader = newDelta;
            }
         }
      }
   }

   void F12020UdpClrMapper::m_UpdateTimeDeltaQualy(DriverData^ reference, int i, bool toPlayer /* if false -> to leader */)
   {
      DriverData^ opponent = Drivers[i];
      if (!opponent->Present)
         return;

      float newDelta = Drivers[i]->FastestLap->Lap - reference->FastestLap->Lap;

      if (toPlayer)
      {
         if (newDelta != opponent->TimedeltaToPlayer)
         {
            opponent->LastTimedeltaToPlayer = opponent->TimedeltaToPlayer;
            opponent->TimedeltaToPlayer = newDelta;
         }
      }
      else
      {
         if (newDelta != opponent->TimedeltaToLeader)
         {
            opponent->TimedeltaToLeader = newDelta;
         }
      }
   }

   void F12020UdpClrMapper::m_UpdateTyre(int i)
   {
      DriverData^ driver = Drivers[i];
      if (!driver->Present)
         return;

      auto tyres = m_parser->status.m_carStatusData[i].m_tyresDamage;
      float tyreStatus = static_cast<float>(tyres[0] + tyres[1] + tyres[2] + tyres[3]);
      tyreStatus /= 400;

      // map 75% -> 100% ... 0% -> 0%
      if (tyreStatus >= 0.75f)
         tyreStatus = 1;
      else
      {
         tyreStatus *= (1.f / 0.75f);
      }
      driver->TyreDamage = tyreStatus;

      driver->WearDetail->WearFrontLeft = m_parser->status.m_carStatusData[i].m_tyresWear[2];
      driver->WearDetail->WearFrontRight = m_parser->status.m_carStatusData[i].m_tyresWear[3];
      driver->WearDetail->WearRearLeft = m_parser->status.m_carStatusData[i].m_tyresWear[0];
      driver->WearDetail->WearRearRight = m_parser->status.m_carStatusData[i].m_tyresWear[1];
   }

   void F12020UdpClrMapper::m_UpdateDamage(int i)
   {
      DriverData^ driver = Drivers[i];
      if (!driver->Present)
         return;

      float damage = m_parser->status.m_carStatusData[i].m_frontLeftWingDamage;
      damage += m_parser->status.m_carStatusData[i].m_frontRightWingDamage;
      damage += m_parser->status.m_carStatusData[i].m_rearWingDamage;
      damage /= 300;

      driver->WearDetail->DamageFrontLeft = m_parser->status.m_carStatusData[i].m_frontLeftWingDamage;
      driver->WearDetail->DamageFrontRight = m_parser->status.m_carStatusData[i].m_frontRightWingDamage;

      // map 50% -> 100% ... 0% -> 0%
      if (damage >= 0.5f)
         damage = 1;
      else
      {
         damage *= (1.f / 0.5f);
      }
      driver->CarDamage = damage;
   }

   void F12020UdpClrMapper::m_UpdateClassification()
   {
      if (Classification != nullptr)
         return;

      if (!m_parser->classification.m_numCars)
         return;

      // classification available, apply:
      Classification = gcnew array<ClassificationData^>(m_parser->classification.m_numCars);

      for (unsigned i = 0; i < Classification->Length; ++i)
      {
         ClassificationData^ pClr = gcnew ClassificationData();
         Classification[i] = pClr;
         FinalClassificationData* pNative = &m_parser->classification.m_classificationData[i];

         pClr->Driver = Drivers[i];
         pClr->BestLapTime = pNative->m_bestLapTime;
         pClr->TotalRaceTime = pNative->m_totalRaceTime;
         pClr->GridPosition = pNative->m_gridPosition;
         pClr->NumLaps = pNative->m_numLaps;
         pClr->NumPenalties = pNative->m_numPenalties;
         pClr->PenaltiesTime = pNative->m_penaltiesTime;
         pClr->Points = pNative->m_points;
         pClr->Position = pNative->m_position;
      }

      m_parser->classification.m_numCars = 0; // set a marker that classifcation results were captured.
   }

   void F12020UdpClrMapper::SetDriverNameMappings(DriverNameMappings^ newMappings)
   {
      m_nameMapings = newMappings;

      // refresh all Names
      for (unsigned i = 0; i < Drivers->Length; ++i)
         m_UpdateDriverName(i);
   }

   void F12020UdpClrMapper::m_UpdateDriverName(int i)
   {
      if (0 == m_parser->participants.m_participants[i].m_raceNumber)
      {
         // no valid data from telemetry present, skip
         return;
      }

      Drivers[i]->SetNameFromTelemetry(m_parser->participants.m_participants[i].m_name);

      // 3 possibilities:
      // 1. Use Mapped name (preferred)
      // 2. Take telemetry name (if it is not generic "Player")
      // 3. Generate name from Team + number

      // 1. check if name mapping is present:
      if (m_nameMapings != nullptr)
      {
         // two pass: first check for team + number match, otherwise check for number only match
         for (unsigned j = 0; j < m_nameMapings->Mappings->Length; ++j)
         {
            if ( 
               (m_nameMapings->Mappings[j]->Team.HasValue) &&
               (m_nameMapings->Mappings[j]->Team.Value == Drivers[i]->Team) &&               
               (m_nameMapings->Mappings[j]->DriverNumber == m_parser->participants.m_participants[i].m_raceNumber))
            {
               Drivers[i]->MappedName = m_nameMapings->Mappings[j]->Name;
               Drivers[i]->Name = Drivers[i]->MappedName;
               return;
            }
         }

         for (unsigned j = 0; j < m_nameMapings->Mappings->Length; ++j)
         {
            if (
               (!m_nameMapings->Mappings[j]->Team.HasValue) &&
               (m_nameMapings->Mappings[j]->DriverNumber == m_parser->participants.m_participants[i].m_raceNumber))
            {
               Drivers[i]->MappedName = m_nameMapings->Mappings[j]->Name;
               Drivers[i]->Name = Drivers[i]->MappedName;
               return;
            }
         }

         Drivers[i]->MappedName = "";
      }

      // 2. & 3.
      if (m_parser->participants.m_participants[i].m_driverId < 100)
      {
         Drivers[i]->Name = Drivers[i]->TelemetryName;
      }
      else
      {
         // online player -> no useful name from telemetry available, so name after team + car number

         String^ pName = "Car";         
         switch (m_parser->participants.m_participants[i].m_teamId)
         {
         case 0: pName = "Mercedes"; break;
         case 1: pName = "Ferrari"; break;
         case 2: pName = "Red Bull"; break;
         case 3: pName = "Williams"; break;
         case 4: pName = "Racing Point"; break;
         case 5: pName = "Renault"; break;
         case 6: pName = "Alpha Tauri"; break;
         case 7: pName = "Haas"; break;
         case 8: pName = "McLaren"; break;
         case 9: pName = "Alfa Romeo"; break;
         }

         pName += " (" + m_parser->participants.m_participants[i].m_raceNumber + ")";

         Drivers[i]->Name = pName;
      }
   }
}

