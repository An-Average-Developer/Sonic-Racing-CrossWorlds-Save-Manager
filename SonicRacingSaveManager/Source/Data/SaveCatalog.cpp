#include "Data/SaveCatalog.h"
#include "Data/ColorCatalog.h"
#include "Data/MusicCatalog.h"
#include <unordered_map>

// Generated from the game exe's enum names (EDriverId, EMachineId...) and the ids seen in saves
// (stickers, honor titles, albums, tracks have plain int ids the exe doesn't list).

namespace SonicRacingSaveManager::Data::SaveCatalog {

const std::vector<std::wstring>& Drivers() {
    static const std::vector<std::wstring> list = {
        L"Sonic", L"Tails", L"Knuckles", L"Amy", L"DrEggman", L"Shadow", L"Rouge", L"Silver", L"Blaze", L"MetalSonic", L"Omega", L"Vector",
        L"Espio", L"Charmy", L"Zavok", L"Zazz", L"Cream", L"Big", L"Eggpawn", L"Sage", L"Jet", L"Wave", L"Storm", L"SonicRacer",
        L"TailsRacer", L"KnucklesRacer", L"AmyRacer", L"DrEggmanRacer", L"ShadowRacer", L"RougeRacer", L"SilverRacer", L"BlazeRacer",
        L"MetalSonicRacer", L"OmegaRacer", L"VectorRacer", L"EspioRacer", L"CharmyRacer", L"ZavokRacer", L"ZazzRacer", L"CreamRacer",
        L"BigRacer", L"EggpawnRacer", L"SageRacer", L"JetRacer", L"WaveRacer", L"StormRacer", L"SuperSonic", L"EggpawnBlue", L"EggpawnRed",
        L"EggpawnYellow", L"EggpawnGreen", L"SageBlack", L"Werehog", L"TailsPrime", L"AmyPrime", L"KnucklesPrime",
        L"Extnd04_Character04001", L"Extnd04_Character04002", L"Extnd04_Character04003", L"Extnd05_Character05001",
        L"Extnd05_Character05002", L"Extnd06_Character06001", L"Extnd06_Character06002", L"Extnd06_Character06003",
        L"Extnd06_Character06004", L"Extnd06_Character06005", L"Extnd07_Character07001", L"Extnd07_Character07002",
        L"Extnd08_Character08001", L"Extnd08_Character08002", L"Extnd08_Character08003", L"Extnd08_Character08004",
        L"Extnd09_Character09001", L"Extnd09_Character09002", L"Extnd10_Character10001", L"Extnd11_Character11002",
        L"Extnd12_Character12001", L"Extnd13_Character13001", L"Extnd13_Character13002", L"Extnd14_Character14001",
        L"Extnd15_Character15001", L"Extnd16_Character16001", L"Extnd17_Character17001", L"Extnd18_Character18001",
        L"Extnd19_Character19001", L"Extnd19_Character19002", L"Extnd20_Character20001", L"Extnd20_Character20002",
        L"Extnd21_Character21001",
        // Season 2 (2.0) DLC (the enum's other new ids have no character yet)
        L"Extnd28_Character28001", L"Extnd30_Character30001", L"Extnd32_Character32001", L"Extnd35_Character35001",
        L"Extnd35_Character35002",
        // Season 2: the AI racers in colour (the Donpa Ticket shop's "Characters", 3 per character)
        L"SonicRacerColor01", L"SonicRacerColor02", L"SonicRacerColor03", L"TailsRacerColor01", L"TailsRacerColor02",
        L"TailsRacerColor03", L"KnucklesRacerColor01", L"KnucklesRacerColor02", L"KnucklesRacerColor03", L"AmyRacerColor01",
        L"AmyRacerColor02", L"AmyRacerColor03", L"DrEggmanRacerColor01", L"DrEggmanRacerColor02", L"DrEggmanRacerColor03",
        L"ShadowRacerColor01", L"ShadowRacerColor02", L"ShadowRacerColor03", L"RougeRacerColor01", L"RougeRacerColor02",
        L"RougeRacerColor03", L"SilverRacerColor01", L"SilverRacerColor02", L"SilverRacerColor03", L"BlazeRacerColor01",
        L"BlazeRacerColor02", L"BlazeRacerColor03", L"MetalSonicRacerColor01", L"MetalSonicRacerColor02", L"MetalSonicRacerColor03",
        L"OmegaRacerColor01", L"OmegaRacerColor02", L"OmegaRacerColor03", L"VectorRacerColor01", L"VectorRacerColor02",
        L"VectorRacerColor03", L"EspioRacerColor01", L"EspioRacerColor02", L"EspioRacerColor03", L"CharmyRacerColor01",
        L"CharmyRacerColor02", L"CharmyRacerColor03", L"ZavokRacerColor01", L"ZavokRacerColor02", L"ZavokRacerColor03",
        L"ZazzRacerColor01", L"ZazzRacerColor02", L"ZazzRacerColor03", L"CreamRacerColor01", L"CreamRacerColor02", L"CreamRacerColor03",
        L"BigRacerColor01", L"BigRacerColor02", L"BigRacerColor03", L"EggpawnRacerColor01", L"EggpawnRacerColor02",
        L"EggpawnRacerColor03", L"SageRacerColor01", L"SageRacerColor02", L"SageRacerColor03", L"JetRacerColor01", L"JetRacerColor02",
        L"JetRacerColor03", L"WaveRacerColor01", L"WaveRacerColor02", L"WaveRacerColor03", L"StormRacerColor01", L"StormRacerColor02",
        L"StormRacerColor03",
    };
    return list;
}

std::wstring DriverFamily(const std::wstring& id) {
    // The AI racers are "<character>Racer"
    // Season 2's AI racers in colour are "<character>RacerColor01..03"
    if (const size_t colour = id.find(L"RacerColor"); colour != std::wstring::npos && colour > 0) return DriverFamily(id.substr(0, colour));
    constexpr std::wstring_view racer = L"Racer";
    if (id.size() > racer.size() && id.ends_with(racer)) return DriverFamily(id.substr(0, id.size() - racer.size()));
    if (id.starts_with(L"Eggpawn")) return L"Eggpawn";
    static const std::unordered_map<std::wstring, std::wstring> versions = {
        { L"SuperSonic", L"Sonic" },     { L"Werehog", L"Sonic" },        { L"Extnd19_Character19001", L"Sonic" },   // Classic Sonic
        { L"Extnd32_Character32001", L"Shadow" },   // Super Shadow (Grand Prix)
        { L"TailsPrime", L"Tails" },     { L"AmyPrime", L"Amy" },         { L"KnucklesPrime", L"Knuckles" },        // Nine, Rusty Rose, Dread
        { L"SageBlack", L"Sage" },       { L"Extnd20_Character20002", L"Extnd20_Character20001" },                  // Classic Amigo
    };
    const auto it = versions.find(id);
    return it != versions.end() ? it->second : id;
}

int TitleFamily(int id) {
    if (id >= 104 && id < 104 + 7 * 23 && (id - 104) % 7 < 4) return 104 + (id - 104) / 7 * 7;
    // A festival's six (EVANGELION's has a seventh, 1597)
    if (id >= 1100 && id < 2000 && id % 10 >= 1 && id % 10 <= 7) return id / 10 * 10 + 4;
    if (id >= 2000 && id <= 2006) return 2000;
    // Season 2: the Donpa Ticket shop's (9001..9015), the friends / squad ones (9900, 9901), the feats (6001, 6002)
    if (id >= 9001 && id <= 9015) return 9001;
    if (id == 9900 || id == 9901) return 9900;
    if (id == 6001 || id == 6002) return 6001;
    return id;
}

int TitleGroup(int id) {
    const int f = TitleFamily(id);
    if (f >= 104 && f < 2000 && f != id) return f < 1100 ? 0 : 1;
    if (f >= 104 && f < 1100 && (f - 104) % 7 == 0) return 0;
    if (f >= 1100 && f < 2000 && f % 10 == 4) return 1;
    return f == 2000 ? 2 : 3;
}

int ChallengeGoal(int number) {
    // DT_ChallengeListData: each row's count, right after its text (challenge_text_NNN)
    static constexpr int goals[] = {
        10, 10, 5, 10, 10, 10, 10, 10, 10, 50, 1, 5, 15, 1,
        1, 1, 1, 8, 8, 23, 1, 1, 10, 10, 10, 10, 10, 8,
        1, 8, 1, 1, 1, 1, 24, 15, 24, 15, 39, 39, 1, 39,
        21, 21, 21, 21, 14, 26, 1, 98, 5, 5, 10, 100, 10, 10,
        10, 1, 4, 7, 40000, 100000, 50000, 24, 100, 78, 240, 100, 8, 1,
    };
    return number >= 1 && number <= static_cast<int>(std::size(goals)) ? goals[number - 1] : 0;
}

// (SPD_08, ACL_10, HND_08 and POW_07 are named in the game but have no machine: no icon, no parts)
const std::vector<std::wstring>& Machines() {
    static const std::vector<std::wstring> list = {
        L"SPD_01", L"SPD_02", L"SPD_03", L"SPD_04", L"SPD_05", L"SPD_06", L"SPD_07", L"SPD_09", L"SPD_10", L"SPD_11", L"SPD_12",
        L"SPD_13", L"SPD_14", L"SPD_15", L"SPD_16", L"SPD_17", L"SPD_18", L"SPD_19", L"SPD_20", L"SPD_51", L"ACL_01", L"ACL_02", L"ACL_03",
        L"ACL_04", L"ACL_05", L"ACL_06", L"ACL_07", L"ACL_08", L"ACL_09", L"ACL_11", L"ACL_12", L"ACL_13", L"ACL_14", L"ACL_15",
        L"ACL_16", L"ACL_17", L"ACL_18", L"ACL_19", L"ACL_20", L"ACL_51", L"HND_01", L"HND_02", L"HND_03", L"HND_04", L"HND_05", L"HND_06",
        L"HND_07", L"HND_09", L"HND_10", L"HND_11", L"HND_12", L"HND_13", L"HND_14", L"HND_15", L"HND_16", L"HND_17", L"HND_18",
        L"HND_19", L"HND_20", L"HND_51", L"POW_01", L"POW_02", L"POW_03", L"POW_04", L"POW_05", L"POW_06", L"POW_08", L"POW_09",
        L"POW_10", L"POW_11", L"POW_12", L"POW_13", L"POW_14", L"POW_15", L"POW_16", L"POW_17", L"POW_18", L"POW_19", L"POW_20", L"POW_51",
        L"DSH_01", L"DSH_02", L"DSH_03", L"DSH_04", L"DSH_05", L"DSH_06", L"DSH_07", L"DSH_08", L"DSH_09", L"DSH_10", L"DSH_11", L"DSH_12",
        L"DSH_13", L"DSH_14", L"DSH_15", L"DSH_16", L"DSH_17", L"DSH_18", L"DSH_19", L"DSH_20", L"DSH_51",
        // Season 2 (2.0) and its DLC (the enum's other new ids have no machine yet)
        L"SPD_23", L"ACL_24", L"ACL_25", L"ACL_26", L"ACL_28", L"ACL_52", L"ACL_53", L"DSH_21", L"DSH_25", L"DSH_26", L"POW_24", L"POW_25",
    };
    return list;
}

const std::vector<std::wstring>& Gadgets() {
    static const std::vector<std::wstring> list = {
        L"Item1_TradeStock", L"Item2_PlusStock", L"Item3_AddWarp", L"Item6_UpLuckAttack", L"Item7_UpLuckDefence", L"Item8_UpLuckSpeed",
        L"Item9_UpLuckTrap", L"Item10_EnhanceRocketPunch", L"Item11_UpLuckWisps", L"Item12_EnhanceThornBall", L"Item13_EnhanceBomb",
        L"Item15_AddDoubleWisps", L"Item16_InitialEquipeDarkchao", L"Item18_InitialEquipeWhiteWisp", L"Item21_InitialEquipeMonsterTruck",
        L"Item22_CreateItemNormal", L"Item23_CreateItemSpeed", L"Item24_CreateItemDefence", L"Item25_CreateItemAttack",
        L"Item26_CreateItemTrap", L"Item27_DoubleItemOccasionally", L"Item28_DoubleItem", L"Item29_DisableItemLost", L"Run1_UltimateDrift",
        L"Run2_QuickAirtrick", L"Run3_SkateDrift", L"Run4_JustDash", L"Run6_FirstFastCharge", L"Run7_SwitchDriftQuickCharge",
        L"Run8_SeconFastCharge", L"Run9_ThirdFastCharge", L"Run10_QuickAirtrickMini", L"Run12_UltimateChargeAttack",
        L"Run13_UltimateChargeIgnoreRunOff", L"Run14_HyperChargeDrift", L"Run15_SpinDrift", L"Run16_ShortenDamageMotion",
        L"Run19_SuppressHitWallDeceleration", L"Run22_EnhanceGrip", L"Run25_SpeedTune", L"Run26_SpeedTuneBeta", L"Run27_AccelTune",
        L"Run28_AccelTuneBeta", L"Run29_HandlingTune", L"Run30_HandlingTuneBeta", L"Run31_PowerTune", L"Run32_PowerTuneBeta",
        L"Run33_DashTune", L"Run34_DashTuneBeta", L"Run35_Starter", L"Run36_SuperStarter", L"Run37_Slowter", L"Run38_SuperSlowter",
        L"Run41_CourseOutGetWisp", L"Run42_EnhanceHookTurn", L"Run43_ImmuneItemDamageOnce", L"Run44_BoatChargeJumpUp",
        L"Various2_TouchDash", L"Various4_EnforceSlipstream", L"Ring1_DashUp", L"Ring2_LostSpeed", L"Ring4_UpperLimit", L"Ring5_Stole",
        L"Ring6_Guard", L"Ring7_BonusDashPanel", L"Ring8_RunoffNegateBonus", L"Ring9_ExpandGetRingRange", L"Ring10_ItemAttackRingBonus",
        L"Ring11_BonusRotateRing", L"Ring12_BonusRotateRingMini", L"Ring13_SlipstreamBonus", L"Ring14_BonusDashPanelMini",
        L"Ring15_BonusDashPanelCombo", L"Ring16_StoleMini", L"Ring17_UpperLimitMini", L"Ring18_DashUpMini", L"Ring19_BonusDriftJust",
        L"Ring20_BonusStartDashSecced", L"Ring21_BonusTravelRing", L"Ring23_BonusTransformAction", L"Various5_LotsOfItems",
        L"Various6_DamageSupport", L"Various7_CollectRings", L"Various8_JumpAction", L"Various9_FastShortCut",
        L"Various10_QuikenDriftCharge", L"Various11_SpeedMachineCustom", L"Various12_AccelMachineCustom", L"Various13_HandleMachineCustom",
        L"Various14_PowerMachineCustom", L"Various15_DashMachineCustom", L"Various16_SpeedCharaCustom", L"Various17_AccelCharaCustom",
        L"Various18_HandleCharaCustom", L"Various19_PowerCharaCustom", L"Various20_DashCharaCustom", L"Various21_RunAhead",
        L"Various22_RunAheadBeta", L"Various23_CatchUp", L"Various25_PlaneOperateCustom", L"Various26_BoatOperateCustom",
        L"Various27_LotsOfWisps", L"Various28_CollectRings2", L"Various29_HandleCharaCustom2", L"Hidden01_2LapGetItem",
        L"Hidden02_3LapGetItem", L"Hidden03_DriftDashBonus", L"Hidden04_ImmuneWeightAndBoomBooOnce", L"Hidden05_PlaneFormParamUP",
        L"Hidden06_BoatFormParamUP", L"Hidden07_ShortenDamageMotionMini", L"Hidden08_ImmuneHitWallDamage", L"Hidden09_SuppressRunoff",
        L"Item4_AddSplash", L"Run23_BoatJumpSplash", L"Ring3_DoubleRing", L"Run5_AddActionBoatPlane", L"Various1_TeamFriendDash",
        L"Various5_TeamEnforceSlipstream", L"ShortenDamageTime", L"LowRankRingBonus", L"ShortenItemSlotTime", L"run45_SpeedConverter",
        L"run46_AccelConverter", L"run47_HandleConverter", L"run48_PowerConverter", L"run49_DashConverter", L"Item30_RecoverItem",
        L"Run50_ImmuneItemDamageFinal", L"Run17_RecoverInvincible", L"Ring12_SlipstreamBonusSuper", L"Ring22_RecoverRing",
        L"Ring24_RingEngine", L"Ring25_HyperRingEngine", L"Ring26_TopBonus", L"Ring27_TravelSelectBonus", L"Item31_EnhanceOmochao",
        L"Run11_UltimateAirTrick", L"Various30_DriftSpinKit", L"Various31_ItemBusterKit", L"Various32_DashComboKit",
        L"Various33_SpeedCharaCustom2", L"Item32_SubstituteItem", L"Run11_UltimateChargeZero", L"Run18_LowCharge",
        L"Run23_SwitchDriftPerfect", L"Run23_SwitchDriftKeeper", L"Run51_TouchCharge", L"Run52_HyperChargeDriftBeta",
        L"Run53_ImmuneItemDamageClimax", L"Run54_TouchEvolution", L"Run55_RingEvolution", L"Run56_DamageEvolution",
        L"Run57_UltimateAirTrick", L"Run58_AirTrickCharge", L"Run59_JustLandingDash", L"Run60_ItemHitEvolution", L"Item33_TrapGetItem",
        L"Item34_FinalDarkChao", L"Hidden10_FourthFastCharge", L"Hidden11_SpinDash", L"Various34_JustChargeKit",
        L"Various35_UltimateChargeKit", L"Various36_AirTrickActionKit", L"Various37_SlipstreamKit", L"Various38_SpinDashKit",
        L"Various39_SpinStartDashKit",
        // Season 2 (2.0) (Item36/38/40 and Hidden12 have no name or data yet)
        L"Item35_DoubleItemRarely", L"Item37_ItemAttackDash", L"Item39_EnhanceOmochaoMini", L"Ring28_BonusDriftJustMini",
        L"Ring29_DoubleRingRarely", L"Ring30_DoubleRingOccasionally", L"Run52_BrakeCharge", L"Run61_ShortenDamageMotion",
        L"Run62_FirstFastChargeMini", L"Run63_TouchDashMini", L"Run64_TransformUp", L"Run65_AirTrickEvolution",
        L"Run66_DashPanelEvolution", L"Run67_OverTakeButton", L"Various40_AccelDashCharaKit", L"Various41_QuikenDriftCharge2",
        L"Various42_DamageSupport2", L"Various43_GiganticItemKit",
    };
    return list;
}

const std::vector<std::wstring>& Horns() {
    static const std::vector<std::wstring> list = {
        L"HORNTYPE_01", L"HORNTYPE_02", L"HORNTYPE_03", L"HORNTYPE_04", L"HORNTYPE_05", L"HORNTYPE_06", L"HORNTYPE_07", L"HORNTYPE_08",
        L"HORNTYPE_09", L"HORNTYPE_10", L"HORNTYPE_11", L"HORNTYPE_12", L"HORNTYPE_13", L"HORNTYPE_14", L"HORNTYPE_15", L"HORNTYPE_16",
        L"HORNTYPE_17", L"HORNTYPE_18", L"HORNTYPE_19", L"HORNTYPE_20", L"HORNTYPE_21", L"HORNTYPE_22", L"HORNTYPE_23", L"HORNTYPE_24",
        L"HORNTYPE_25", L"HORNTYPE_26", L"HORNTYPE_27", L"HORNTYPE_28", L"HORNTYPE_29", L"HORNTYPE_30", L"HORNTYPE_31", L"HORNTYPE_32",
        L"HORNTYPE_33", L"HORNTYPE_34", L"HORNTYPE_35", L"HORNTYPE_36", L"HORNTYPE_37", L"HORNTYPE_38", L"HORNTYPE_39",
        // Season 2 (2.0): the Duel Medal exchange's
        L"HORNTYPE_40", L"HORNTYPE_41", L"HORNTYPE_42", L"HORNTYPE_43", L"HORNTYPE_44", L"HORNTYPE_45", L"HORNTYPE_46", L"HORNTYPE_47",
        L"HORNTYPE_48", L"HORNTYPE_49",
        L"HORNTYPE_EXTND_01", L"HORNTYPE_EXTND_02", L"HORNTYPE_EXTND_03", L"HORNTYPE_EXTND_04", L"HORNTYPE_EXTND_05", L"HORNTYPE_EXTND_06",
        L"HORNTYPE_EXTND_07", L"HORNTYPE_EXTND_08", L"HORNTYPE_EXTND_09", L"HORNTYPE_EXTND_10", L"HORNTYPE_EXTND_11", L"HORNTYPE_EXTND_12",
        L"HORNTYPE_EXTND_13", L"HORNTYPE_EXTND_14", L"HORNTYPE_EXTND_15", L"HORNTYPE_EXTND_16", L"HORNTYPE_EXTND_17", L"HORNTYPE_EXTND_18",
        L"HORNTYPE_EXTND_19", L"HORNTYPE_EXTND_20", L"HORNTYPE_EXTND_21", L"HORNTYPE_EXTND_22", L"HORNTYPE_EXTND_23", L"HORNTYPE_EXTND_24",
        L"HORNTYPE_EXTND_25", L"HORNTYPE_EXTND_26", L"HORNTYPE_EXTND_27", L"HORNTYPE_EXTND_28", L"HORNTYPE_EXTND_29", L"HORNTYPE_EXTND_30",
        L"HORNTYPE_EXTND_31", L"HORNTYPE_EXTND_32", L"HORNTYPE_EXTND_33", L"HORNTYPE_EXTND_34", L"HORNTYPE_EXTND_35", L"HORNTYPE_EXTND_36",
        L"HORNTYPE_EXTND_37", L"HORNTYPE_EXTND_38", L"HORNTYPE_EXTND_39", L"HORNTYPE_EXTND_40", L"HORNTYPE_EXTND_41",
    };
    return list;
}

const std::vector<std::wstring>& Auras() {
    static const std::vector<std::wstring> list = {
        L"MACHINE_AURA_NONE", L"MACHINE_AURA_001", L"MACHINE_AURA_002", L"MACHINE_AURA_003", L"MACHINE_AURA_004", L"MACHINE_AURA_005",
        L"MACHINE_AURA_006", L"MACHINE_AURA_007", L"MACHINE_AURA_008", L"MACHINE_AURA_009", L"MACHINE_AURA_010", L"MACHINE_AURA_011",
        L"MACHINE_AURA_012", L"MACHINE_AURA_013", L"MACHINE_AURA_014", L"MACHINE_AURA_015", L"MACHINE_AURA_016", L"MACHINE_AURA_017",
        L"MACHINE_AURA_018", L"MACHINE_AURA_019", L"MACHINE_AURA_020",
        // 021..030: empty slots of the enum (no name, no effect in the game's aura tables)
        // Season 2 (2.0): Thunderclap, Wings, Lightning and five plain colours (057..066 have no data yet)
        L"MACHINE_AURA_031", L"MACHINE_AURA_032", L"MACHINE_AURA_033", L"MACHINE_AURA_034", L"MACHINE_AURA_035", L"MACHINE_AURA_036",
        L"MACHINE_AURA_037", L"MACHINE_AURA_038", L"MACHINE_AURA_039", L"MACHINE_AURA_040", L"MACHINE_AURA_041", L"MACHINE_AURA_042",
        L"MACHINE_AURA_043", L"MACHINE_AURA_044", L"MACHINE_AURA_045", L"MACHINE_AURA_046", L"MACHINE_AURA_047", L"MACHINE_AURA_048",
        L"MACHINE_AURA_049", L"MACHINE_AURA_050", L"MACHINE_AURA_051", L"MACHINE_AURA_052", L"MACHINE_AURA_053", L"MACHINE_AURA_054",
        L"MACHINE_AURA_055", L"MACHINE_AURA_056",
    };
    return list;
}

const std::vector<std::wstring>& ColorPresets() {
    // The ones the game has (its DT_MachineColorPreset tables; the enum has many more slots)
    static const std::vector<std::wstring> list = [] {
        std::vector<std::wstring> ids;
        for (const auto& c : ColorCatalog::Presets()) ids.push_back((c.id < 10 ? L"ColorPreset0" : L"ColorPreset") + std::to_wstring(c.id));
        return ids;
    }();
    return list;
}

const std::vector<std::wstring>& Challenges() {
    static const std::vector<std::wstring> list = {
        L"CHALLENGE001", L"CHALLENGE002", L"CHALLENGE003", L"CHALLENGE004", L"CHALLENGE005", L"CHALLENGE006", L"CHALLENGE007",
        L"CHALLENGE008", L"CHALLENGE009", L"CHALLENGE010", L"CHALLENGE011", L"CHALLENGE012", L"CHALLENGE013", L"CHALLENGE014",
        L"CHALLENGE015", L"CHALLENGE016", L"CHALLENGE017", L"CHALLENGE018", L"CHALLENGE019", L"CHALLENGE020", L"CHALLENGE021",
        L"CHALLENGE022", L"CHALLENGE023", L"CHALLENGE024", L"CHALLENGE025", L"CHALLENGE026", L"CHALLENGE027", L"CHALLENGE028",
        L"CHALLENGE029", L"CHALLENGE030", L"CHALLENGE031", L"CHALLENGE032", L"CHALLENGE033", L"CHALLENGE034", L"CHALLENGE035",
        L"CHALLENGE036", L"CHALLENGE037", L"CHALLENGE038", L"CHALLENGE039", L"CHALLENGE040", L"CHALLENGE041", L"CHALLENGE042",
        L"CHALLENGE043", L"CHALLENGE044", L"CHALLENGE045", L"CHALLENGE046", L"CHALLENGE047", L"CHALLENGE048", L"CHALLENGE049",
        L"CHALLENGE050", L"CHALLENGE051", L"CHALLENGE052", L"CHALLENGE053", L"CHALLENGE054", L"CHALLENGE055", L"CHALLENGE056",
        L"CHALLENGE057", L"CHALLENGE058", L"CHALLENGE059", L"CHALLENGE060", L"CHALLENGE061", L"CHALLENGE062", L"CHALLENGE063",
        L"CHALLENGE064", L"CHALLENGE065", L"CHALLENGE066", L"CHALLENGE067", L"CHALLENGE068", L"CHALLENGE069", L"CHALLENGE070",
    };
    return list;
}

const std::vector<int>& Stickers() {
    // DT_StickerData01..06: every sticker the game has (the ids the save keeps)
    static const std::vector<int> list = {
        0, 1, 10000, 10001, 10002, 10003, 10004, 10005, 10006, 10007, 10008, 10009, 10010, 10011, 10012, 10013, 10014, 10015,
        10016, 10017, 10018, 10019, 10020, 10021, 10022, 10023, 10024, 10025, 10026, 10027, 10028, 10029, 10030, 10031, 10032,
        10033, 10034, 10035, 10036, 10037, 10038, 10039, 10040, 10041, 10042, 10043, 10044, 10045, 10046, 10047, 10048, 10049,
        10050, 10051, 10052, 10053, 10054, 10055, 10056, 10057, 10058, 10059, 10060, 10061, 10062, 10063, 10064, 10065, 10066,
        10067, 10068, 10069, 10070, 10071, 10072, 10073, 10074, 10075, 10076, 10077, 10078, 10079, 10080, 10081, 10082, 10083,
        10084, 10085, 10086, 10087, 10088, 10089, 10090, 10100, 20000, 20001, 20002, 20003, 20004, 20005, 20100, 20101, 20102,
        20103, 20104, 20105, 20200, 20201, 20202, 20203, 20204, 20205, 20300, 20301, 20302, 20303, 20304, 20305, 20400, 20401,
        20402, 20403, 20404, 20405, 20500, 20501, 20502, 20503, 20504, 20505, 20600, 20601, 20602, 20603, 20604, 20605, 20700,
        20701, 20702, 20703, 20704, 20705, 20800, 20801, 20802, 20803, 20804, 20805, 20900, 20901, 20902, 20903, 20904, 20905,
        20906, 20907, 21000, 21001, 21002, 21003, 21004, 21005, 21100, 21101, 21102, 21103, 21104, 21105, 21200, 21201, 21202,
        21203, 21204, 21205, 21300, 21301, 21302, 21303, 21304, 21305, 21400, 21401, 21402, 21403, 21404, 21405, 21500, 21501,
        21502, 21503, 21504, 21505, 21600, 21601, 21602, 21603, 21604, 21605, 21606, 21607, 21608, 21609, 21610, 21611, 21612,
        21613, 21614, 21615, 21616, 21617, 21618, 21700, 21701, 21702, 21703, 21704, 21705, 21706, 21707, 21800, 21801, 21802,
        21803, 21804, 21805, 21806, 21807, 21900, 21901, 21902, 21903, 21904, 21905, 21906, 22000, 22001, 22002, 22003, 22004,
        22005, 22100, 22101, 22102, 22103, 22104, 22105, 22106, 22107, 22200, 22201, 22202, 22203, 22205, 22300, 22301, 22302,
        22303, 22304, 22305, 22306, 22307, 22308, 22309, 22310, 22311, 22312, 22313, 22314, 22400, 22401, 22402, 22403, 30000,
        30001, 30002, 30003, 30004, 30005, 30006, 30007, 30008, 30009, 30010, 30011, 30012, 30013, 30014, 30015, 30016, 40000,
        40001, 40002, 40003, 40004, 40005, 40006, 40007, 40008, 40009, 40010, 40011, 40012, 40013, 40014, 40015, 40016, 40017,
        40018, 40019, 40020, 40021, 40022, 40023, 40024, 40025, 40026, 40027, 40028, 40029, 40030, 40031, 40032, 40033, 40034,
        40035, 40036, 40037, 40038, 40039, 40040, 40041, 40042, 40043, 40044, 40045, 40046, 40047, 40048, 40049, 40050, 40051,
        40052, 40054, 40055, 40056, 40057, 40058, 40059, 40061, 40062, 40063, 40064, 40065, 40067, 40068, 40069, 40070, 40071,
        40072, 40073, 40074, 40075, 40076, 40077, 40078, 40079, 40080, 40081, 40082, 40083, 40084, 40085, 40086, 40087, 40088,
        40089, 40090, 40091, 40092, 40093, 40094, 40095, 40096, 40097, 40098, 40099, 40100, 40101, 40102, 40103, 40104, 40105,
        40106, 40107, 40108, 40109, 40110, 40111, 40112, 40113, 40114, 40115, 40116, 40117, 40118, 40119, 40120, 40121, 40122,
        40123, 40124, 40125, 40126, 40127, 40128, 56101, 56102, 56103, 56104, 56201, 56202, 56203, 56204, 56302, 56303, 56304,
        56305, 56306, 56307, 56308, 56400, 56401, 56402, 56403, 60000, 60001, 60010, 60011, 60012, 60020, 60021, 60022, 60023,
        60024, 60030, 60031, 60032, 60033, 60034, 60040, 60041, 60042, 60050, 60051, 60052, 60053, 60054, 60060, 60061, 60062,
        60070, 60080, 60081, 60082, 60083, 60090, 60091, 60092, 60093, 60100, 60101, 60110, 60111, 60120, 60121, 60122, 60123,
        60124, 60125, 60126, 60127, 60130, 60131, 60140, 60141, 60142, 60143, 60144, 60145, 60150, 60160, 60161, 60162, 60163,
        60164, 60165, 60166, 60170, 60171, 60172, 60173, 60174, 60175, 60180, 60181, 60182, 60183, 60190, 60191, 60192, 60193,
        60194, 60195, 60196, 60197, 60200, 60201, 60202, 60203, 60204, 60205, 60206, 60217, 60218, 60219, 60220, 60221, 60222,
        60223, 60224, 60225, 60226, 60227, 60228, 60240, 60241, 60242, 60243, 60244, 60250, 60251, 60252, 60253, 60254, 60255,
        60256, 60257, 60260, 60261, 60262, 60263, 60264, 60265, 60266, 60267, 60268, 60270, 60271, 60272, 60280, 60281, 60282,
        60283, 60284, 60285, 60286, 60290, 60291, 60292, 60300, 60301, 60302, 61026, 61027, 61028, 61029, 61030, 61031, 61032,
        70000, 70001, 70002, 70003, 70004, 70005, 70006, 70007, 70008, 70009, 70010, 70011, 70012, 70013, 70014, 70015, 70016,
        70017, 70018, 70019, 70020, 70021, 70022, 70023, 70024, 70025, 70026, 70027, 70028, 70029, 70030, 70031, 70032, 70033,
        70034, 70035, 70036, 70037, 70038, 70039, 70040, 70041, 70042, 70043, 70044, 70045, 70046, 70047, 70048, 70049, 70050,
        70051, 70052, 70053, 70054, 70055, 70056, 70057, 70058, 70059, 70060, 70061, 70062, 70063, 70064, 70065, 70066, 70067,
        70068, 70069, 70070, 70071, 70072, 70073, 70074,
    };
    return list;
}

const std::vector<int>& HonorTitles() {
    // DT_HonorTitleListDataTable_01..06: every title the game has
    static const std::vector<int> list = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
        62, 63, 64, 65, 66, 67, 68, 69, 70, 104, 105, 106, 107, 111, 112, 113, 114, 118, 119, 120, 121, 125, 126, 127, 128, 132,
        133, 134, 135, 139, 140, 141, 142, 146, 147, 148, 149, 153, 154, 155, 156, 160, 161, 162, 163, 167, 168, 169, 170, 174,
        175, 176, 177, 181, 182, 183, 184, 188, 189, 190, 191, 195, 196, 197, 198, 202, 203, 204, 205, 209, 210, 211, 212, 216,
        217, 218, 219, 223, 224, 225, 226, 230, 231, 232, 233, 237, 238, 239, 240, 244, 245, 246, 247, 251, 252, 253, 254, 258,
        259, 260, 261, 302, 303, 304, 305, 306, 307, 308, 1101, 1102, 1103, 1104, 1105, 1106, 1111, 1112, 1113, 1114, 1115,
        1116, 1121, 1122, 1123, 1124, 1125, 1126, 1131, 1132, 1133, 1134, 1135, 1136, 1141, 1142, 1143, 1144, 1145, 1146, 1151,
        1152, 1153, 1154, 1155, 1156, 1161, 1162, 1163, 1164, 1165, 1166, 1171, 1172, 1173, 1174, 1175, 1176, 1181, 1182, 1183,
        1184, 1185, 1186, 1191, 1192, 1193, 1194, 1195, 1196, 1201, 1202, 1203, 1204, 1205, 1206, 1211, 1212, 1213, 1214, 1215,
        1216, 1301, 1302, 1303, 1304, 1305, 1306, 1311, 1312, 1313, 1314, 1315, 1316, 1321, 1322, 1323, 1324, 1325, 1326, 1501,
        1502, 1503, 1504, 1505, 1506, 1511, 1512, 1513, 1514, 1515, 1516, 1521, 1522, 1523, 1524, 1525, 1526, 1531, 1532, 1533,
        1534, 1535, 1536, 1541, 1542, 1543, 1544, 1545, 1546, 1551, 1552, 1553, 1554, 1555, 1556, 1561, 1562, 1563, 1564, 1565,
        1566, 1581, 1582, 1583, 1584, 1585, 1586, 1591, 1592, 1593, 1594, 1595, 1596, 1597, 1741, 1742, 1743, 1744, 1745, 1746,
        2000, 2001, 2002, 2003, 2004, 2005, 2006, 6001, 6002, 9001, 9002, 9003, 9004, 9005, 9006, 9007, 9008, 9009, 9010, 9011,
        9012, 9013, 9014, 9015, 9900, 9901,
    };
    return list;
}

// The jukebox's albums and tracks (MusicCatalog, from the game's tables)
const std::vector<int>& Albums() {
    static const std::vector<int> list = [] {
        std::vector<int> ids;
        for (const auto& a : MusicCatalog::Albums()) ids.push_back(a.id);
        return ids;
    }();
    return list;
}

const std::vector<int>& Tracks() {
    static const std::vector<int> list = [] {
        std::vector<int> ids;
        for (const auto& t : MusicCatalog::Tracks()) ids.push_back(t.id);
        return ids;
    }();
    return list;
}

} // namespace SonicRacingSaveManager::Data::SaveCatalog
