/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2024 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/appearance/mounts/mounts.hpp"

#include "config/configmanager.hpp"
#include "game/game.hpp"
#include "utils/pugicast.hpp"
#include "utils/tools.hpp"

bool Mounts::reload() {
	mounts.clear();
	return loadFromXml();
}

bool Mounts::loadFromXml() {
	pugi::xml_document doc;
	auto folder = g_configManager().getString(CORE_DIRECTORY) + "/XML/mounts.xml";
	pugi::xml_parse_result result = doc.load_file(folder.c_str());
	if (!result) {
		printXMLError(__FUNCTION__, folder, result);
		return false;
	}

	for (auto mountNode : doc.child("mounts").children()) {
		auto lookType = pugi::cast<uint16_t>(mountNode.attribute("clientid").value());
		if (g_configManager().getBoolean(WARN_UNSAFE_SCRIPTS) && lookType != 0 && !g_game().isLookTypeRegistered(lookType)) {
			g_logger().warn("{} - An unregistered creature mount with id '{}' was blocked to prevent client crash.", __FUNCTION__, lookType);
			continue;
		}

		mounts.emplace(std::make_shared<Mount>(
			static_cast<uint8_t>(pugi::cast<uint16_t>(mountNode.attribute("id").value())),
			lookType,
			mountNode.attribute("name").as_string(),
			pugi::cast<int32_t>(mountNode.attribute("speed").value()),
			mountNode.attribute("premium").as_bool(),
			mountNode.attribute("type").as_string()
		));
	}
	return true;
}

std::shared_ptr<Mount> Mounts::getMountByID(uint8_t id) {
	auto it = std::find_if(mounts.begin(), mounts.end(), [id](const std::shared_ptr<Mount> &mount) {
		return mount->id == id; // Note the use of -> operator to access the members of the Mount object
	});

	return it != mounts.end() ? *it : nullptr; // Returning the shared_ptr to the Mount object
}

std::shared_ptr<Mount> Mounts::getMountByName(const std::string &name) {
	auto mountName = name.c_str();
	auto it = std::find_if(mounts.begin(), mounts.end(), [mountName](const std::shared_ptr<Mount> &mount) {
		return strcasecmp(mountName, mount->name.c_str()) == 0;
	});

	return it != mounts.end() ? *it : nullptr;
}

std::shared_ptr<Mount> Mounts::getMountByClientID(uint16_t clientId) {
	auto it = std::find_if(mounts.begin(), mounts.end(), [clientId](const std::shared_ptr<Mount> &mount) {
		return mount->clientId == clientId; // Note the use of -> operator to access the members of the Mount object
	});

	return it != mounts.end() ? *it : nullptr; // Returning the shared_ptr to the Mount object
}

bool Mounts::addAttributes(uint32_t playerId, uint8_t mountId) {
	const auto &player = g_game().getPlayerByID(playerId);
	if (!player) {
		return false;
	}

	auto mount = getMountByID(mountId);
	if (!mount) {
		g_logger().warn("[Mounts::addAttributes] Mount with ID {} not found.", mountId);
		return false;
	}

	// Apply Conditions
	if (mount->manaShield) {
		const auto &condition = Condition::createCondition(CONDITIONID_MOUNT, CONDITION_MANASHIELD, -1, 0);
		player->addCondition(condition);
	}

	if (mount->invisible) {
		const auto &condition = Condition::createCondition(CONDITIONID_MOUNT, CONDITION_INVISIBLE, -1, 0);
		player->addCondition(condition);
	}

	if (mount->regeneration) {
		const auto &condition = Condition::createCondition(CONDITIONID_MOUNT, CONDITION_REGENERATION, -1, 0);
		if (mount->healthGain) {
			condition->setParam(CONDITION_PARAM_HEALTHGAIN, mount->healthGain);
		}

		if (mount->healthTicks) {
			condition->setParam(CONDITION_PARAM_HEALTHTICKS, mount->healthTicks);
		}

		if (mount->manaGain) {
			condition->setParam(CONDITION_PARAM_MANAGAIN, mount->manaGain);
		}

		if (mount->manaTicks) {
			condition->setParam(CONDITION_PARAM_MANATICKS, mount->manaTicks);
		}

		player->addCondition(condition);
	}

	// Apply skills
	for (uint32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i) {
		if (mount->skills[i]) {
			player->setVarSkill(static_cast<skills_t>(i), mount->skills[i]);
		}
	}

	// Apply life leech
	if (mount->lifeLeechChance > 0) {
		player->setVarSkill(SKILL_LIFE_LEECH_CHANCE, mount->lifeLeechChance);
	}

	if (mount->lifeLeechAmount > 0) {
		player->setVarSkill(SKILL_LIFE_LEECH_AMOUNT, mount->lifeLeechAmount);
	}

	// Apply mana leech
	if (mount->manaLeechChance > 0) {
		player->setVarSkill(SKILL_MANA_LEECH_CHANCE, mount->manaLeechChance);
	}

	if (mount->manaLeechAmount > 0) {
		player->setVarSkill(SKILL_MANA_LEECH_AMOUNT, mount->manaLeechAmount);
	}

	// Apply critical hit
	if (mount->criticalChance > 0) {
		player->setVarSkill(SKILL_CRITICAL_HIT_CHANCE, mount->criticalChance);
	}

	if (mount->criticalDamage > 0) {
		player->setVarSkill(SKILL_CRITICAL_HIT_DAMAGE, mount->criticalDamage);
	}

	// Apply stats
	for (uint32_t s = STAT_FIRST; s <= STAT_LAST; ++s) {
		if (mount->stats[s]) {
			player->setVarStats(static_cast<stats_t>(s), mount->stats[s]);
		}
	}

	player->sendStats();
	player->sendSkills();
	return true;
}

bool Mounts::removeAttributes(uint32_t playerId, uint8_t mountId) {
	const auto &player = g_game().getPlayerByID(playerId);
	if (!player) {
		return false;
	}

	auto it = std::find_if(mounts.begin(), mounts.end(), [mountId](const std::shared_ptr<Mount> &mount) {
		return mount->id == mountId;
	});

	auto mount = *it;

	if (!mount) {
		g_logger().warn("[Mounts::removeAttributes] Mount with ID {} not found.", mountId);
		return false;
	}

	// Remove conditions
	if (mount->manaShield) {
		player->removeCondition(CONDITION_MANASHIELD, CONDITIONID_MOUNT);
	}

	if (mount->invisible) {
		player->removeCondition(CONDITION_INVISIBLE, CONDITIONID_MOUNT);
	}

	if (mount->regeneration) {
		player->removeCondition(CONDITION_REGENERATION, CONDITIONID_MOUNT);
	}

	// Revert Skills
	for (uint32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i) {
		if (mount->skills[i]) {
			player->setVarSkill(static_cast<skills_t>(i), -mount->skills[i]);
		}
	}

	// Revert Life Leech
	if (mount->lifeLeechChance > 0) {
		player->setVarSkill(SKILL_LIFE_LEECH_CHANCE, -mount->lifeLeechChance);
	}

	if (mount->lifeLeechAmount > 0) {
		player->setVarSkill(SKILL_LIFE_LEECH_AMOUNT, -mount->lifeLeechAmount);
	}

	// Revert Mana Leech
	if (mount->manaLeechChance > 0) {
		player->setVarSkill(SKILL_MANA_LEECH_CHANCE, -mount->manaLeechChance);
	}

	if (mount->manaLeechAmount > 0) {
		player->setVarSkill(SKILL_MANA_LEECH_AMOUNT, -mount->manaLeechAmount);
	}

	// Revert Critical Hit
	if (mount->criticalChance > 0) {
		player->setVarSkill(SKILL_CRITICAL_HIT_CHANCE, -mount->criticalChance);
	}

	if (mount->criticalDamage > 0) {
		player->setVarSkill(SKILL_CRITICAL_HIT_DAMAGE, -mount->criticalDamage);
	}

	// Revert Stats
	for (uint32_t s = STAT_FIRST; s <= STAT_LAST; ++s) {
		if (mount->stats[s]) {
			player->setVarStats(static_cast<stats_t>(s), -mount->stats[s]);
		}
	}

	player->sendStats();
	player->sendSkills();
	return true;
}