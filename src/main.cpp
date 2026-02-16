#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/MenuGameLayer.hpp>

using namespace geode::prelude;

bool s_enabled;
bool s_brighterGlow;
bool s_enableRobotSpider;

$execute {
    s_enabled = Mod::get()->getSettingValue<bool>("blending-glow");
    s_brighterGlow = Mod::get()->getSettingValue<bool>("brighter-glow");
    s_enableRobotSpider = Mod::get()->getSettingValue<bool>("enable-robot-spider");

    listenForAllSettingChanges([](std::string_view key, std::shared_ptr<SettingV3> setting) {
        s_enabled = Mod::get()->getSettingValue<bool>("blending-glow");
        s_brighterGlow = Mod::get()->getSettingValue<bool>("brighter-glow");
        s_enableRobotSpider = Mod::get()->getSettingValue<bool>("enable-robot-spider");
    });
}

class $modify(ModPlayerObject, PlayerObject) {
    struct Fields {
        CCSprite* m_robotSpiderGlow = nullptr;
    };

    $override
    void updatePlayerArt() {
        PlayerObject::updatePlayerArt();
        if (!m_hasGlow || !s_enabled) return;

        ccBlendFunc blendFunc = {GL_ONE, GL_ONE_MINUS_CONSTANT_ALPHA};

        auto iconGlow = m_iconGlow;
        auto vehicleGlow = m_vehicleGlow;

        if (iconGlow) iconGlow->setBlendFunc(blendFunc);
        if (vehicleGlow) vehicleGlow->setBlendFunc(blendFunc);

        updateRobotGlowScheduler();
    }

    $override
    void updateGlowColor() {
        if (s_brighterGlow) {
            auto gm = GameManager::sharedState();
            auto glowColor = gm->colorForIdx(gm->getPlayerGlowColor());

            int r = std::min(static_cast<int>(glowColor.r * 1.5), 255);
            int g = std::min(static_cast<int>(glowColor.g * 1.5), 255);
            int b = std::min(static_cast<int>(glowColor.b * 1.5), 255);

            m_glowColor = ccc3(r, g, b);
        }

        PlayerObject::updateGlowColor();
    }

    void updateRobotGlowScheduler() {
        if (!s_enableRobotSpider) return;

        auto selector = schedule_selector(ModPlayerObject::updateRobotGlow);

        if (m_isRobot || m_isSpider) {
            schedule(selector);
        } else {
            unschedule(selector);
            if (m_fields->m_robotSpiderGlow) m_fields->m_robotSpiderGlow->removeFromParent();
            m_fields->m_robotSpiderGlow = nullptr;
        }
    }

    void updateRobotGlow(float dt) {
        GJRobotSprite* robotSprite = nullptr;
        if (m_isRobot) robotSprite = m_robotSprite;
        else if (m_isSpider) robotSprite = m_spiderSprite;
        else return;

        // for some reason, trying to use a render texture on the glow sprite itself
        // causes the limbs in the rendered sprite to be super messed up, so instead we
        // render the parent batch node and just make everything besides the glow invisible

        auto robotParent = robotSprite->getParent();
        robotParent->setPosition({40, 40});

        CCArrayExt<CCSpritePart*> robotParts = robotSprite->m_paSprite->m_spriteParts;

        for (auto part : robotParts) {
            part->setVisible(false);
        }

        robotSprite->m_glowSprite->setVisible(true);

        auto renderTexture = CCRenderTexture::create(80, 80);
        renderTexture->begin();
        robotParent->visit();
        renderTexture->end();

        robotParent->setPosition({0, 0});

        for (auto part : robotParts) {
            part->setVisible(true);
        }

        robotSprite->m_glowSprite->setVisible(false);

        auto sprite = CCSprite::createWithTexture(renderTexture->getSprite()->getTexture());
        sprite->setFlipY(true);
        sprite->setBlendFunc({GL_ONE, GL_ONE_MINUS_CONSTANT_ALPHA});

        if (m_fields->m_robotSpiderGlow) m_fields->m_robotSpiderGlow->removeFromParent();
        m_fields->m_robotSpiderGlow = sprite;
        m_mainLayer->addChild(sprite);
    }
};

// when the editor playtest is paused, robots & spiders still animate but the scheduler is paused
// this causes the glow sprite to not update, so we need to manually resume it

class $modify(ModEditorUI, EditorUI) {
    $override
    void onPlaytest(CCObject* sender) {
        EditorUI::onPlaytest(sender);
        if (!s_enableRobotSpider) return;

        auto player1 = LevelEditorLayer::get()->m_player1;
        if (player1 && (player1->m_isRobot || player1->m_isSpider)) player1->resumeSchedulerAndActions();

        auto player2 = LevelEditorLayer::get()->m_player2;
        if (player2 && (player2->m_isRobot || player2->m_isSpider)) player2->resumeSchedulerAndActions();
    }
};

// when the icon in MenuGameLayer changes modes, it doesn't call PlayerObject::updatePlayerArt
// so we need to manually update the robot/spider glow sprite

class $modify(ModMenuGameLayer, MenuGameLayer) {
    $override
    void resetPlayer() {
        MenuGameLayer::resetPlayer();
        static_cast<ModPlayerObject*>(m_playerObject)->updateRobotGlowScheduler();
    }
};
