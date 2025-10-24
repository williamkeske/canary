/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2024 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once
#include "creatures/creatures_definitions.hpp"

enum PlayerSex_t : uint8_t;
class Player;

struct OutfitEntry {
	constexpr explicit OutfitEntry(uint16_t initLookType, uint8_t initAddons) :
		lookType(initLookType), addons(initAddons) { }

	uint16_t lookType;
	uint8_t addons;
};

struct Outfit {
	Outfit(std::string initName, std::string initFrom, bool initPremium, bool initUnlocked, uint16_t initLookType) :
		name(std::move(initName)), from(std::move(initFrom)), premium(initPremium), unlocked(initUnlocked), lookType(initLookType) {
		std::memset(skills, 0, sizeof(skills));
		std::memset(stats, 0, sizeof(stats));
	}

	std::string name = "";
	std::string from = "";

	bool premium = false;
	bool unlocked = false;
	bool manaShield = false;
	bool invisible = false;
	bool regeneration = false;

	uint16_t lookType = 0;

	int32_t speed = 0;
	int32_t attackSpeed = 0;
	int32_t healthGain = 0;
	int32_t healthTicks = 0;
	int32_t manaGain = 0;
	int32_t manaTicks = 0;

	double lifeLeechChance = 0;
	double lifeLeechAmount = 0;
	double manaLeechChance = 0;
	double manaLeechAmount = 0;
	double criticalChance = 0;
	double criticalDamage = 0;

	int32_t skills[SKILL_LAST + 1] = { 0 };
	int32_t stats[STAT_LAST + 1] = { 0 };
};

struct ProtocolOutfit {
	ProtocolOutfit(const std::string &initName, uint16_t initLookType, uint8_t initAddons) :
		name(initName), lookType(initLookType), addons(initAddons) { }

	const std::string &name;
	uint16_t lookType;
	uint8_t addons;
};

class Outfits {
public:
	static Outfits &getInstance();

	bool reload();
	bool loadFromXml();

	[[nodiscard]] std::shared_ptr<Outfit> getOutfitByLookType(const std::shared_ptr<const Player> &player, uint16_t lookType, bool isOppositeOutfit = false) const;
	[[nodiscard]] const std::vector<std::shared_ptr<Outfit>> &getOutfits(PlayerSex_t sex) const;

	std::shared_ptr<Outfit> getOutfitByName(PlayerSex_t sex, const std::string &name) const;
	uint32_t getOutfitId(PlayerSex_t sex, uint16_t lookType) const;

	bool addAttributes(uint32_t playerId, uint32_t outfitId, uint16_t sex, uint16_t addons);
	bool removeAttributes(uint32_t playerId, uint32_t outfitId, uint16_t sex);
};
