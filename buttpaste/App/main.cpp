#include "../Source/Bootstrap.h"
int main()
{
	{

		if (!g_backend->setup())
			return std::getchar();

		if (!g_backend->attach(target_process)) {
			//logging::print(encrypt("Failed to attach process.\n"));
			g_backend->unload();
			return std::getchar();
		}
		Logger->print_hex("Base Address: ", g_backend->m_base_address);

		Overlays::HijackOverlay();
		Render::StartImgui();
		std::thread roundStateThread([]() {
			while (true) {
				EntityState::GetGameState();
				Sleep(10);
			}
			});

		roundStateThread.detach();
		std::thread([]() { EntityState::CachePlayers(); }).detach();
		std::thread([]() {
			
				if (GetAsyncKeyState(VK_F5) & 0x8000) {
					PatchEngine::SetupActor();
					PatchEngine::SetupCamera();
					cout << "[+] Patch Offsets" << endl;
					Sleep(200);
				}
				if (GetAsyncKeyState(VK_F6) & 0x8000) {
					PatchEngine::RestoreActor();
					PatchEngine::RestoreCamera();
					cout << "[+] Restore Patches" << endl;
					Sleep(200);
				}
				if (GetAsyncKeyState(VK_F7) & 0x8000) {
					PatchEngine::ClearCodeCaves();
					cout << "[+] Clear Codecaves" << endl;
					Sleep(200);
				}
				Sleep(10);

			}).detach();

		StartRender::StartRendering();
		return getchar();
	}
}
