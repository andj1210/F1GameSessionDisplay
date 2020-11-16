// Copyright 2018-2020 Andreas Jung
// SPDX-License-Identifier: GPL-3.0-only

using namespace System;
using namespace System::Runtime::InteropServices;
using namespace System::Net;
using namespace System::Net::Sockets;

#include "F12020DataDefs.h"
#include "F12020DataDefsClr.h"
#include "F12020ElementaryParser.h"
#include <cassert>
#include <random>
#include <algorithm>

namespace adjsw::F12020
{
   public ref class F12020Parser
   {
   public:
      F12020Parser(String^ ip, int port);
      ~F12020Parser();

      bool Work();

      // insert some data to display, only for debugging!
      void InsertTestData();

      property SessionInfo^ SessionInfo;
      property SessionEventList^ EventList;
      property int CountDrivers;
      property array<DriverData^>^ Drivers;      

   private:
      void m_Clear();

      void m_Update();
      void m_UpdateEvent();
      void m_UpdateDrivers();
      void m_UpdateTimeDelta(DriverData^ player, int i);
      void m_UpdateTyre(int i);
      void m_UpdateDamage(int i);
      void m_UpdateTelemetry(int i);

      IPEndPoint^ m_ep;
      F12020ElementaryParser* m_parser;
      UdpClient^ m_socket;
      array<Byte>^ arr;
      IntPtr pUnmanaged;
      int len;
   };

   F12020Parser::F12020Parser(String^ ip, int port)
   {
      m_ep = gcnew IPEndPoint(IPAddress::Parse(ip), port);
      m_socket = gcnew UdpClient(port, AddressFamily::InterNetwork);
      m_parser = new F12020ElementaryParser();
      arr = gcnew array<Byte>(4096);
      len = 0;
      pUnmanaged = Marshal::AllocHGlobal(64 * 1024);

      Drivers = gcnew array<DriverData^>(22);

      for (int i = 0; i < Drivers->Length; ++i)
         Drivers[i] = gcnew DriverData();

      SessionInfo = gcnew adjsw::F12020::SessionInfo();
      EventList = gcnew SessionEventList();
   }

   void F12020Parser::m_UpdateTelemetry(int i)
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

   bool F12020Parser::Work()
   {
      try
      {
         if (m_socket->Available == 0)
            return false;

         arr = m_socket->Receive(m_ep);
         len = arr->Length;
      }
      catch (...)
      {
         return false;
      }

      if (len > (64*1024))
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

   void F12020Parser::InsertTestData()
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

   void F12020Parser::m_Clear()
   {
      SessionInfo->SessionFinshed = false;
      SessionInfo->CurrentLap = 1;
      EventList->Events->Clear();
      EventList = EventList; // force NPC
      CountDrivers = 0;

      for each (DriverData^ dat in Drivers)
      {
         dat->Reset();
      }
   }


   void F12020Parser::m_Update()
   {
      m_UpdateEvent();
      m_UpdateDrivers();
   }

   void F12020Parser::m_UpdateEvent()
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

   void F12020Parser::m_UpdateDrivers()
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
         if (String::IsNullOrEmpty(Drivers[i]->Name))
         {
            if (m_parser->participants.m_participants[i].m_raceNumber)
            {
               // implies participant data is present
               if (m_parser->participants.m_participants[i].m_driverId >= 100)
               {
                  const char* pName = "Car";
                  // online player -> no useful name, so name after team + car number
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

                  char playername[48];
                  snprintf(playername, 48, "%s (%u)", pName, (unsigned)m_parser->participants.m_participants[i].m_raceNumber);
                  Drivers[i]->SetName(playername);
               }
               else
               {
                  Drivers[i]->SetName(m_parser->participants.m_participants[i].m_name);
               }
            }
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

      if (m_parser->lap.m_header.m_playerCarIndex > Drivers->Length)
         return; // comes in visitor modes


      DriverData^ player = Drivers[m_parser->lap.m_header.m_playerCarIndex];

      // m_parser->lap.m_header.m_playerCarIndex defaults to 0 and might change when the first actual packet arrives
      // which means we must check, if we declared first car 0 by accident as player and revert in that case!
      if (m_parser->lap.m_header.m_playerCarIndex != 0)
         Drivers[0]->IsPlayer = false;

      player->IsPlayer = true;
      player->TimedeltaToPlayer = 0;

      if (!player->LapNr)
         return;

      // update the delta Time, tyre and car damage
      for (int i = 0; i < Drivers->Length; ++i)
      {
         DriverData^ car = Drivers[i];
         if (!car->Present)
            continue;

         if (!car->IsPlayer)
            m_UpdateTimeDelta(player, i);

         m_UpdateTelemetry(i);
         m_UpdateTyre(i);
         m_UpdateDamage(i);

         car->PenaltySeconds = m_parser->lap.m_lapData[i].m_penalties;
         car->Tyre = F1Tyre(m_parser->status.m_carStatusData[i].m_actualTyreCompound);
         car->VisualTyre = F1VisualTyre(m_parser->status.m_carStatusData[i].m_visualTyreCompound);
         if (!car->VisualTyres->Count && (static_cast<int>(car->VisualTyre) != 0))
         {
            // add the first tyre at the start of race
            car->VisualTyres->Add(car->VisualTyre);
            car->VisualTyres = car->VisualTyres; // trigger NotifyPorpertyChanged
         }

         // car->TyreAge = m_parser->status.m_carStatusData[i].m_tyresAgeLaps; -> NOOO its a lie!

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

   void F12020Parser::m_UpdateTimeDelta(DriverData^ player, int i)
   {
      DriverData^ opponent = Drivers[i];
      if (opponent->IsPlayer || (!opponent->Present))
         return;

      // player passed Sector 2:
      bool found = false;
      int lapIdx = player->LapNr - 1;
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
            if ((player->Laps[lapIdx]->Sector1) && opponent->Laps[lapIdx]->Sector1)
            {
               found = true;
            }
            break;

         case 1:
            if ((player->Laps[lapIdx]->Sector2) && opponent->Laps[lapIdx]->Sector2)
            {
               found = true;
            }
            break;

         case 2:
            if ((player->Laps[lapIdx]->Lap) && opponent->Laps[lapIdx]->Lap)
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
               timePlayer = player->Laps[lapIdx - 1]->LapsAccumulated;
               timeOpponent = opponent->Laps[lapIdx - 1]->LapsAccumulated;
            }

            switch (lapSector)
            {
            case 0:
               timePlayer += player->Laps[lapIdx]->Sector1;
               timeOpponent += opponent->Laps[lapIdx]->Sector1;
               break;

            case 1:
               timePlayer += player->Laps[lapIdx]->Sector1 + player->Laps[lapIdx]->Sector2;
               timeOpponent += opponent->Laps[lapIdx]->Sector1 + opponent->Laps[lapIdx]->Sector2;
               break;

            case 2:
               timePlayer += player->Laps[lapIdx]->Lap;
               timeOpponent += opponent->Laps[lapIdx]->Lap;
               break;
            }
         }
         timePlayer += player->PenaltySeconds;
         auto newDelta = timePlayer - timeOpponent;
         newDelta -= m_parser->lap.m_lapData[i].m_penalties;
         if (newDelta != opponent->TimedeltaToPlayer)
         {
            opponent->LastTimedeltaToPlayer = opponent->TimedeltaToPlayer;
            opponent->TimedeltaToPlayer = newDelta;
         }
      }

   }

   void F12020Parser::m_UpdateTyre(int i)
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

   void F12020Parser::m_UpdateDamage(int i)
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

   F12020Parser::~F12020Parser()
   {
      delete m_parser;
      Marshal::FreeHGlobal(pUnmanaged);
   }

}

