/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2024 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/appearance/outfit/outfit.hpp"
#include "creatures/combat/condition.hpp"
#include "creatures/creatures_definitions.hpp"
#include "config/configmanager.hpp"
#include "creatures/players/player.hpp"
#include "game/game.hpp"
#include "lib/di/container.hpp"
#include "utils/pugicast.hpp"
#include "utils/tools.hpp"

std::vector<std::shared_ptr<Outfit>> outfits[PLAYERSEX_LAST + 1];

Outfits &Outfits::getInstance() {
	return inject<Outfits>();
}

bool Outfits::reload() {
	for (auto &outfitsVector : outfits) {
		outfitsVector.clear();
	}
	return loadFromXml();
}

bool Outfits::loadFromXml() {
	pugi::xml_document doc;
	auto folder = g_configManager().getString(CORE_DIRECTORY) + "/XML/outfits.xml";
	pugi::xml_parse_result result = doc.load_file(folder.c_str());
	if (!result) {
		printXMLError(__FUNCTION__, folder, result);
		return false;
	}

	for (auto outfitNode : doc.child("outfits").children()) {
		pugi::xml_attribute attr;
		if ((attr = outfitNode.attribute("enabled")) && !attr.as_bool()) {
			continue;
		}

		if (!(attr = outfitNode.attribute("type"))) {
			g_logger().warn("[Outfits::loadFromXml] - Missing outfit type");
			continue;
		}

		auto type = pugi::cast<uint16_t>(attr.value());
		if (type > PLAYERSEX_LAST) {
			g_logger().warn("[Outfits::loadFromXml] - Invalid outfit type {}", type);
			continue;
		}

		pugi::xml_attribute lookTypeAttribute = outfitNode.attribute("looktype");
		if (!lookTypeAttribute) {
			g_logger().warn("[Outfits::loadFromXml] - Missing looktype on outfit");
			continue;
		}

		if (auto lookType = pugi::cast<uint16_t>(lookTypeAttribute.value());
		    g_configManager().getBoolean(WARN_UNSAFE_SCRIPTS) && lookType != 0
		    && !g_game().isLookTypeRegistered(lookType)) {
			g_logger().warn("[Outfits::loadFromXml] An unregistered creature looktype type with id '{}' was ignored to prevent client crash.", lookType);
			continue;
		}

		auto outfit = std::make_shared<Outfit>(
			outfitNode.attribute("name").as_string(),
			outfitNode.attribute("from").as_string(),
			outfitNode.attribute("premium").as_bool(),
			outfitNode.attribute("unlocked").as_bool(true),
			pugi::cast<uint16_t>(lookTypeAttribute.value())
		);
		
		outfit->manaShield = outfitNode.attribute("manaShield").as_bool() || outfitNode.attribute("manashield").as_bool();
		outfit->invisible = outfitNode.attribute("invisible").as_bool();
		outfit->speed = outfitNode.attribute("speed").as_int();
		outfit->attackSpeed = outfitNode.attribute("attackSpeed").as_int() || outfitNode.attribute("attackspeed").as_int();

		if (auto healthGainAttr = outfitNode.attribute("healthGain")) {
			outfit->healthGain = healthGainAttr.as_int();
			outfit->regeneration = true;
		}

		if (auto healthTicksAttr = outfitNode.attribute("healthTicks")) {
			outfit->healthTicks = healthTicksAttr.as_int();
			outfit->regeneration = true;
		}

		if (auto manaGainAttr = outfitNode.attribute("manaGain")) {
			outfit->manaGain = manaGainAttr.as_int();
			outfit->regeneration = true;
		}

		if (auto manaTicksAttr = outfitNode.attribute("manaTicks")) {
			outfit->manaTicks = manaTicksAttr.as_int();
			outfit->regeneration = true;
		}

		if (auto skillsNode = outfitNode.child("skills")) {
			for (auto skillNode : skillsNode.children()) {
				std::string skillName = skillNode.name();
				int32_t skillValue = skillNode.attribute("value").as_int();

				if (skillName == "fist") {
					outfit->skills[SKILL_FIST] += skillValue;
				} else if (skillName == "club") {
					outfit->skills[SKILL_CLUB] += skillValue;
				} else if (skillName == "axe") {
					outfit->skills[SKILL_AXE] += skillValue;
				} else if (skillName == "sword") {
					outfit->skills[SKILL_SWORD] += skillValue;
				} else if (skillName == "distance" || skillName == "dist") {
					outfit->skills[SKILL_DISTANCE] += skillValue;
				} else if (skillName == "shielding" || skillName == "shield") {
					outfit->skills[SKILL_SHIELD] = skillValue;
				} else if (skillName == "fishing" || skillName == "fish") {
					outfit->skills[SKILL_FISHING] += skillValue;
				} else if (skillName == "melee") {
					outfit->skills[SKILL_FIST] += skillValue;
					outfit->skills[SKILL_CLUB] += skillValue;
					outfit->skills[SKILL_SWORD] += skillValue;
					outfit->skills[SKILL_AXE] += skillValue;
				} else if (skillName == "weapon" || skillName == "weapons") {
					outfit->skills[SKILL_CLUB] += skillValue;
					outfit->skills[SKILL_SWORD] += skillValue;
					outfit->skills[SKILL_AXE] += skillValue;
					outfit->skills[SKILL_DISTANCE] += skillValue;
				}
			}
		}

		if (auto statsNode = outfitNode.child("stats")) {
			for (auto statNode : statsNode.children()) {
				std::string statName = statNode.name();
				int32_t statValue = statNode.attribute("value").as_int();

				if (statName == "maxHealth" || statName == "maxhealth") {
					outfit->stats[STAT_MAXHITPOINTS] += statValue;
				} else if (statName == "maxMana" || statName == "maxmana") {
					outfit->stats[STAT_MAXMANAPOINTS] += statValue;
				} else if (statName == "cap" || statName == "capacity") {
					outfit->stats[STAT_CAPACITY] += statValue * 100;
				} else if (statName == "magLevel" || statName == "magicLevel" || statName == "magiclevel" || statName == "ml") {
					outfit->stats[STAT_MAGICPOINTS] += statValue;
				}
			}
		}

		if (auto imbuingNode = outfitNode.child("imbuing")) {
			for (auto imbuing : imbuingNode.children()) {
				std::string imbuingName = imbuing.name();
				double imbuingValue = imbuing.attribute("value").as_double() * 100.0;

				if (imbuingName == "lifeLeechChance" || imbuingName == "lifeleechchance") {
					outfit->lifeLeechChance += imbuingValue;
				} else if (imbuingName == "lifeleechAmount" || imbuingName == "lifeleechamount") {
					outfit->lifeLeechAmount += imbuingValue;
				} else if (imbuingName == "manaLeechChance" || imbuingName == "manaleechchance") {
					outfit->manaLeechChance += imbuingValue;
				} else if (imbuingName == "manaLeechAmount" || imbuingName == "manaleechamount") {
					outfit->manaLeechAmount += imbuingValue;
				} else if (imbuingName == "criticalChance" || imbuingName == "criticalchance") {
					outfit->criticalChance += imbuingValue;
				} else if (imbuingName == "criticalDamage" || imbuingName == "criticaldamage") {
					outfit->criticalDamage += imbuingValue;
				}
			}
		}

		outfits[type].emplace_back(outfit);
	}
	for (uint8_t sex = PLAYERSEX_FEMALE; sex <= PLAYERSEX_LAST; ++sex) {
		outfits[sex].shrink_to_fit();
	}
	return true;
}

std::shared_ptr<Outfit> Outfits::getOutfitByLookType(const std::shared_ptr<const Player> &player, uint16_t lookType, bool isOppositeOutfit) const {
	if (!player) {
		g_logger().error("[{}] - Player not found", __FUNCTION__);
		return nullptr;
	}

	auto sex = player->getSex();
	if (sex != PLAYERSEX_FEMALE && sex != PLAYERSEX_MALE) {
		g_logger().error("[{}] - Sex invalid or player: {}", __FUNCTION__, player->getName());
		return nullptr;
	}

	if (isOppositeOutfit) {
		sex = (sex == PLAYERSEX_MALE) ? PLAYERSEX_FEMALE : PLAYERSEX_MALE;
	}

	auto it = std::ranges::find_if(outfits[sex], [&lookType](const auto &outfit) {
		return outfit->lookType == lookType;
	});

	if (it != outfits[sex].end()) {
		return *it;
	}
	return nullptr;
}

const std::vector<std::shared_ptr<Outfit>> &Outfits::getOutfits(PlayerSex_t sex) const {
	return outfits[sex];
}

std::shared_ptr<Outfit> Outfits::getOutfitByName(PlayerSex_t sex, const std::string &name) const {
	for (const auto &outfit : outfits[sex]) {
		if (outfit->name == name) {
			return outfit;
		}
	}

	return nullptr;
}

uint32_t Outfits::getOutfitId(PlayerSex_t sex, uint16_t lookType) const {
	for (const auto &outfit : outfits[sex]) {
		if (outfit->lookType == lookType) {
			return outfit->lookType;
		}
	}

	return 0;
}

bool Outfits::addAttributes(uint32_t playerId, uint32_t outfitId, uint16_t sex, uint16_t addons) {
	const auto &player = g_game().getPlayerByID(playerId);
	if (!player) {
		return false;
	}

	auto &outfitsList = outfits[sex];
	auto it = std::ranges::find_if(outfitsList, [&outfitId](const auto &outfit) {
		return outfit->lookType == outfitId;
	});

	if (it == outfitsList.end()) {
		return false;
	}

	const auto &outfit = *it;

	// Apply Conditions
	if (outfit->manaShield) {
		const auto &condition = Condition::createCondition(CONDITIONID_OUTFIT, CONDITION_MANASHIELD, -1, 0);
		player->addCondition(condition);
	}

	if (outfit->invisible) {
		const auto &condition = Condition::createCondition(CONDITIONID_OUTFIT, CONDITION_INVISIBLE, -1, 0);
		player->addCondition(condition);
	}

	if (outfit->speed) {
		g_game().changeSpeed(player, outfit->speed);
	}

	if (outfit->regeneration) {
		const auto &condition = Condition::createCondition(CONDITIONID_OUTFIT, CONDITION_REGENERATION, -1, 0);
		if (outfit->healthGain) {
			condition->setParam(CONDITION_PARAM_HEALTHGAIN, outfit->healthGain);
		}

		if (outfit->healthTicks) {
			condition->setParam(CONDITION_PARAM_HEALTHTICKS, outfit->healthTicks);
		}

		if (outfit->manaGain) {
			condition->setParam(CONDITION_PARAM_MANAGAIN, outfit->manaGain);
		}

		if (outfit->manaTicks) {
			condition->setParam(CONDITION_PARAM_MANATICKS, outfit->manaTicks);
		}

		player->addCondition(condition);
	}

	// Apply skills
	for (uint32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i) {
		if (outfit->skills[i]) {
			player->setVarSkill(static_cast<skills_t>(i), outfit->skills[i]);
		}
	}

	// Apply life leech
	if (outfit->lifeLeechChance > 0) {
		player->setVarSkill(SKILL_LIFE_LEECH_CHANCE, outfit->lifeLeechChance);
	}

	if (outfit->lifeLeechAmount > 0) {
		player->setVarSkill(SKILL_LIFE_LEECH_AMOUNT, outfit->lifeLeechAmount);
	}

	// Apply mana leech
	if (outfit->manaLeechChance > 0) {
		player->setVarSkill(SKILL_MANA_LEECH_CHANCE, outfit->manaLeechChance);
	}

	if (outfit->manaLeechAmount > 0) {
		player->setVarSkill(SKILL_MANA_LEECH_AMOUNT, outfit->manaLeechAmount);
	}

	// Apply critical hit
	if (outfit->criticalChance > 0) {
		player->setVarSkill(SKILL_CRITICAL_HIT_CHANCE, outfit->criticalChance);
	}

	if (outfit->criticalDamage > 0) {
		player->setVarSkill(SKILL_CRITICAL_HIT_DAMAGE, outfit->criticalDamage);
	}

	// Apply stats
	for (uint32_t s = STAT_FIRST; s <= STAT_LAST; ++s) {
		if (outfit->stats[s]) {
			player->setVarStats(static_cast<stats_t>(s), outfit->stats[s]);
		}
	}

	player->sendStats();
	player->sendSkills();
	return true;
}

bool Outfits::removeAttributes(uint32_t playerId, uint32_t outfitId, uint16_t sex) {
	const auto &player = g_game().getPlayerByID(playerId);
	if (!player) {
		return false;
	}

	auto &outfitsList = outfits[sex];
	auto it = std::ranges::find_if(outfitsList, [&outfitId](const auto &outfit) {
		return outfit->lookType == outfitId;
	});

	if (it == outfitsList.end()) {
		return false;
	}

	const auto &outfit = *it;

	// Remove conditions
	if (outfit->manaShield) {
		player->removeCondition(CONDITION_MANASHIELD, CONDITIONID_OUTFIT);
	}

	if (outfit->invisible) {
		player->removeCondition(CONDITION_INVISIBLE, CONDITIONID_OUTFIT);
	}

	if (outfit->speed) {
		g_game().changeSpeed(player, -outfit->speed);
	}

	if (outfit->regeneration) {
		player->removeCondition(CONDITION_REGENERATION, CONDITIONID_OUTFIT);
	}

	// Remove skills
	for (uint32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i) {
		if (outfit->skills[i]) {
			player->setVarSkill(static_cast<skills_t>(i), -outfit->skills[i]);
		}
	}

	// Remove life leech
	if (outfit->lifeLeechChance > 0) {
		player->setVarSkill(SKILL_LIFE_LEECH_CHANCE, -outfit->lifeLeechChance);
	}

	if (outfit->lifeLeechAmount > 0) {
		player->setVarSkill(SKILL_LIFE_LEECH_AMOUNT, -outfit->lifeLeechAmount);
	}

	// Remove mana leech
	if (outfit->manaLeechChance > 0) {
		player->setVarSkill(SKILL_MANA_LEECH_CHANCE, -outfit->manaLeechChance);
	}

	if (outfit->manaLeechAmount > 0) {
		player->setVarSkill(SKILL_MANA_LEECH_AMOUNT, -outfit->manaLeechAmount);
	}

	// Remove critical hit
	if (outfit->criticalChance > 0) {
		player->setVarSkill(SKILL_CRITICAL_HIT_CHANCE, -outfit->criticalChance);
	}

	if (outfit->criticalDamage > 0) {
		player->setVarSkill(SKILL_CRITICAL_HIT_DAMAGE, -outfit->criticalDamage);
	}

	// Remove stats
	for (uint32_t s = STAT_FIRST; s <= STAT_LAST; ++s) {
		if (outfit->stats[s]) {
			player->setVarStats(static_cast<stats_t>(s), -outfit->stats[s]);
		}
	}

	player->sendStats();
	player->sendSkills();
	return true;
}
