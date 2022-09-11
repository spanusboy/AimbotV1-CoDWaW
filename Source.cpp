#define _USE_MATH_DEFINES //M_PI

#include <Windows.h>
#include <cmath>
#include <algorithm>
#include <vector>

const DWORD CharacterBaseAddress = 0x18E745C;
const DWORD CharacterOffset = 0x88;
const DWORD CharacterXPosOffset = 0x0;
const DWORD CharacterYPosOffset = 0x4;
const DWORD CharacterZPosOffset = 0x8;
const DWORD CharacterDeadOffset = 0x68;
const int CharacterDeadValue = 0;

const DWORD Maxcharacters = 25;
const DWORD PlayerAngleVertical = 0x18ED18C;
const DWORD PlayerAngleHorizontal = 0x18ED190;
const DWORD PlayerXPos = 0x18ED088;
const DWORD PlayerYPos = 0x18ED08C;
const DWORD PlayerZPos = 0x18ED090;

const DWORD CheatOpCodeBase = 0x00418545;
char* originalOpCodes = "\xF3\x0F\x5C\xC1\xF3\x0F\x59\x05\x3C\x9F\x83\x00\xF3\x0F\x11\x84\xB7\x24\x01\x00\x00\x83\xC6\x01\x83\xFE\x03\x0F\x8C\x36\xFF\xFF\xFF";
char* newOpCodes = "\xF3\x0F\x10\x05\x8C\xD1\x8E\x01\xF3\x0F\x11\x87\x24\x01\x00\x00\xF3\x0F\x10\x05\x90\xD1\x8E\x01\xF3\x0F\x11\x87\x28\x01\x00\x00\x90";
int OpCodeLenght = 33;

const int AimbotMinimumZombieDistance = 500;

void read_memory(DWORD source_, LPVOID destination_, int size_)
{
	memcpy(destination_, (LPVOID)source_, size_);
}

void write_memory(DWORD address_, LPVOID data_, int size_)
{
	DWORD old_protection;
	VirtualProtect((LPVOID)address_, size_, PAGE_EXECUTE_READWRITE, &old_protection);
	memcpy((LPVOID)address_, data_, size_);
	VirtualProtect((LPVOID)address_, size_, old_protection, NULL);
}

void enable_hack()
{
	//escolhendo op codes
	write_memory(CheatOpCodeBase, newOpCodes, OpCodeLenght);
}

void disable_hack()
{
	write_memory(CheatOpCodeBase, originalOpCodes, OpCodeLenght);
}

float calculate_3d_distance(float x1_, float y1_, float z1_, float x2_, float y2_, float z2_)
{
	return std::sqrt((x1_ - x2_)*(x1_ - x2_) + (y1_ - y2_)*(y1_ - y2_) + (z1_ - z2_)*(z1_ - z2_));
}

void calculate_angles(float* angles_, float x1_, float y1_, float z1_, float x2_, float y2_, float z2_)
{
	float delta[3] = {(x1_ - x2_), (y1_ - y2_), (z1_ - z2_)};
	double distance = std::sqrt(std::pow(x1_ - x2_, 2.0) + std::pow(y1_ - y2_, 2.0));
	angles_[0] = (float) (std::atanf(delta[2]/distance) * 180 / M_PI) + 800/distance;
	angles_[1] = (float) (atanf(delta[1]/delta[0]) * 180 / M_PI) + 100/distance;
	if (delta[0] >= 0.0)
	{
		angles_[1] + 180.0f;
	}
}

void aim_towards_angles(float vertical_, float horizontal_)
{
	write_memory(PlayerAngleVertical, &vertical_, sizeof(vertical_));
	write_memory(PlayerAngleHorizontal, &horizontal_, sizeof(horizontal_));
}

struct Player
{
	float XPos;
	float YPos;
	float ZPos;

	void fillData()
	{
		read_memory(PlayerXPos, &XPos, sizeof(XPos));
		read_memory(PlayerYPos, &YPos, sizeof(YPos));
		read_memory(PlayerZPos, &ZPos, sizeof(ZPos));
	}
};

struct Zombie
{
	DWORD BaseAddress;
	float XPos;
	float YPos;
	float ZPos;
	bool Alive;

	void fillData(int character_number_)
	{
		//Calculando endereço base
		BaseAddress = CharacterBaseAddress + character_number_ * CharacterOffset;

		//calculando internos
		update();
	}

	void update()
	{
		if (BaseAddress == NULL)
			return;

		//Determina se o personagem esta vivo ou não
		int temp;
		read_memory(BaseAddress + CharacterDeadOffset, &temp, sizeof(temp));
		if (temp == 0)
			Alive = false;
		else
			Alive = true;

		//pegando xyz do personagem
		read_memory(BaseAddress + CharacterXPosOffset, &XPos, sizeof(XPos));
		read_memory(BaseAddress + CharacterYPosOffset, &YPos, sizeof(YPos));
		read_memory(BaseAddress + CharacterZPosOffset, &ZPos, sizeof(ZPos));
	}
};

struct Target
{
	Player* player;
	Zombie* target;
	float distance;

	void calculate(Player* player_, Zombie* target_)
	{
		this->player = player_;
		this->target = target_;
		distance = calculate_3d_distance(player->XPos, player->YPos, player->ZPos, target->XPos, target->YPos, target->ZPos);
	}
};

bool compare_targets(Target* lhs_, Target* rhs_)
{
	return lhs_->distance > rhs_->distance;
}

DWORD WINAPI main_thread(LPVOID paramter_)
{
	Player player;
	Zombie zombies[Maxcharacters];
	std::vector<Target*> targets;
	bool Active = false;

	while (true)
	{
		if (GetAsyncKeyState(VK_MENU))
		{
			Active = !Active;
		}
		if (Active)
		{
			//atualiza todos os zumbis
			player.fillData();
			for (int i = 0; i < Maxcharacters; i++)
				zombies[i].fillData(i);

			//Pegar alvos
			for (int i = 0; i < Maxcharacters; i++)
			{
				if (zombies[i].Alive)
				{
					targets.push_back(new Target);
					targets.back()->calculate(&player, &(zombies[i]));
				}
			}

			//se nao tiver alvos ele reseta o loop
			if (targets.size() == 0)
				continue;

			//vai sortear o que estiver mais perto
			std::sort(targets.begin(), targets.end(), compare_targets);
			Target* nearestTarget = targets.back();

			//vai pegar o angulono alvo se a distancia for menor que AimbotMinimumZombieDistance
			if (nearestTarget->distance < AimbotMinimumZombieDistance)
			{
				enable_hack();
				float angles[2];
				calculate_angles(angles, nearestTarget->player->XPos, nearestTarget->player->YPos, nearestTarget->player->ZPos, nearestTarget->target->XPos, nearestTarget->target->YPos, nearestTarget->target->ZPos);
				aim_towards_angles(angles[0], angles[1]);
			}
			else
				disable_hack();

			//limpar e deletar memoria alocada para todos os alvos
			for (int i = 0; i < targets.size(); i++)
			{
				Target* tmp = targets.back();
				delete tmp;
				targets.pop_back();
			}
		}
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		CreateThread(0, 0x100, &main_thread, 0, 0, NULL);
	}
	return TRUE;
}