#pragma once
#include "../../../Source/Bootstrap.h"
#include <algorithm>
#include <deque>
namespace EntityState
{
    void CachePlayers()
    {
        printf("[DEBUG] CachePlayers() started\n");
        if (!signature_scanner::auto_find_offsets(
            &offsets::ActorPatch,
            &offsets::CameraPatch,
            &offsets::CodeCaveOne,
            &offsets::CodeCaveTwo,
            offsets::ActorMov))
        {
            printf("[ERROR] Failed to find offsets automatically!\n");
            return;
        }

        unsigned char cam_orig[8]    = { 0 };
        unsigned char actor_orig[16] = { 0 };
        g_backend->read_memory(offsets::CameraPatch, cam_orig,   sizeof(cam_orig));
        g_backend->read_memory(offsets::ActorPatch,  actor_orig, sizeof(actor_orig));

        int actor_ps = PatchEngine::CalcPatchSize(actor_orig, 7);
        PatchEngine::InitBackups(cam_orig, actor_orig, actor_ps);

        //PatchEngine::SetupCamera();
        printf("[DEBUG] SetupCamera() completed\n");
        //PatchEngine::SetupActor();
        printf("[DEBUG] SetupActor() completed\n");

        DWORD last_print     = GetTickCount();
        DWORD last_refresh   = GetTickCount();
        DWORD last_fast      = GetTickCount();
        bool  patches_restored = false;

        for (;;)
        {
            uint64_t CameraAddress = g_backend->read<uint64_t>(offsets::CodeCaveOne);
            if (CameraAddress && CameraAddress != Cached.CameraPointer)
            {
                Cached.CameraPointer = CameraAddress;
                printf("[Camera] CameraPointer: 0x%llX\n", Cached.CameraPointer);
            }

            uint64_t EntityListPtr = g_backend->read<uint64_t>(offsets::CodeCaveTwo);

            if (GetTickCount() - last_print >= 1000)
            {
                printf("[EntityList] CodeCaveTwo: 0x%llX\n", EntityListPtr);
                last_print = GetTickCount();
            }

            if (EntityListPtr > 0x1000 && EntityListPtr < 0x7FFFFFFFFFFF)
            {
                if (!patches_restored)
                {
                    PatchEngine::RestoreActor();
                    PatchEngine::RestoreCamera();
                    patches_restored = true;
                    printf("[EntityList] Pointer valid — patches restored\n");
                }

                if (GetTickCount() - last_refresh >= 2000)
                {
                    last_refresh = GetTickCount();

                    std::vector<uint64_t>   new_actors;
                    std::vector<ActorEntry> new_entries;
                    new_actors.reserve(256);
                    new_entries.reserve(256);

                    int null_streak = 0;
                    for (uint64_t i = 0; i < 15000; i++)
                    {
                        uint64_t entholderzoidisthegoat = g_backend->read<uint64_t>(EntityListPtr + i * 8);
                        if (!SDK::IsValidPtr2(entholderzoidisthegoat)) {
                            if (++null_streak > 64) break;
                            continue;
                        }
                        null_streak = 0;

                        uint64_t ent = g_backend->read<uint64_t>(entholderzoidisthegoat);
                        if (!SDK::IsValidPtr2(ent)) continue;

                        ActorEntry e;
                        e.ptr  = ent;
                        e.fb   = g_backend->read<uint64_t>(ent + 0xB8);
                        e.pos50 = g_backend->read<Vector3>(ent + 0x50);
                        e.pos60 = g_backend->read<Vector3>(ent + 0x60);
                        e.direction = g_backend->read<float>(ent + 0x30);

                        if (SDK::IsActorPlayer(e.fb)) {
                            Cached.ActorPointer = ent;
                            e.pos = SDK::GetActorPosition(ent);
                            printf("[Player] [%llu] 0x%llX  fb=0x%llX\n", i, ent, e.fb);
                        } else {
                            bool v50 = SDK::IsValidWorldPos(e.pos50);
                            bool v60 = SDK::IsValidWorldPos(e.pos60);
                            if (v60 && !v50)       e.pos = e.pos60;
                            else if (v50 && !v60)  e.pos = e.pos50;
                            else if (v50 && v60)   e.pos = (fabsf(e.pos60.z) > fabsf(e.pos50.z)) ? e.pos60 : e.pos50;
                        }

                        new_actors.push_back(ent);
                        new_entries.push_back(e);
                    }

                    Cached.Actors      = std::move(new_actors);
                    Cached.ActorEntries = std::move(new_entries);
                    printf("[EntityList] Refreshed — %zu actors\n", Cached.Actors.size());
                }

                // Fast per-frame update: positions, fb (destroyed state), direction
                if (GetTickCount() - last_fast >= 16)
                {
                    last_fast = GetTickCount();
                    for (auto& e : Cached.ActorEntries)
                    {
                        e.fb        = g_backend->read<uint64_t>(e.ptr + 0xB8);
                        e.direction = g_backend->read<float>(e.ptr + 0x30);

                        if (SDK::IsActorPlayer(e.fb))
                        {
                            e.pos = SDK::GetActorPosition(e.ptr);
                        }
                        else
                        {
                            e.pos50 = g_backend->read<Vector3>(e.ptr + 0x50);
                            e.pos60 = g_backend->read<Vector3>(e.ptr + 0x60);
                            bool v50 = SDK::IsValidWorldPos(e.pos50);
                            bool v60 = SDK::IsValidWorldPos(e.pos60);
                            if      (v60 && !v50) e.pos = e.pos60;
                            else if (v50 && !v60) e.pos = e.pos50;
                            else if (v50 && v60)  e.pos = (fabsf(e.pos60.z) > fabsf(e.pos50.z)) ? e.pos60 : e.pos50;
                        }
                    }
                }
            }
            else
            {
                patches_restored = false;
            }

            Sleep(1);
        }
    }

    void GetGameState()
    {
        int last_round_state = -1;

        for (;;)
        {
            int round_state = SDK::GetRoundState();

            if (round_state != last_round_state)
            {
                // Clear all actor/camera state on every state transition
                Cached.Actors.clear();
                Cached.CameraPointer = 0;
                SDK::s_comp_array_offset = 0;
                printf("[GameState] State %d -> %d — cleared actors, camera\n",
                       last_round_state, round_state);

                switch (round_state)
                {
                case 0:
                    printf("[GameState] RoundSwap\n");
                    Sleep(1000);
                    break;

                case 1:
                    printf("[GameState] OperatorSelection — applying patches\n");
                    break;

                case 2:
                    printf("[GameState] PreparationPhase — applying patches\n");
                    PatchEngine::SetupCamera();
                    PatchEngine::SetupActor();
                    break;

                case 3:
                    printf("[GameState] ActionPhase — applying patches\n");
                    PatchEngine::SetupCamera();
                    PatchEngine::SetupActor();
                    break;

                case 4:
                    printf("[GameState] RoundEnd — clearing code caves\n");
                    PatchEngine::ClearCodeCaves();
                    break;

                case 5:
                    printf("[GameState] MainMenu\n");
                    PatchEngine::ClearCodeCaves();
                    break;

                default:
                    printf("[GameState] Unknown state: %d\n", round_state);
                    Sleep(1000);
                    break;
                }

                last_round_state = round_state;
            }

            Cached.GameState = round_state;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
