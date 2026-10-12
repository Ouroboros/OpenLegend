#pragma once

#include <array>
#include <string_view>

namespace openlegend::text::game_strings {

inline constexpr std::u8string_view unknown = u8"未知";
inline constexpr std::u8string_view person_open = u8"人（";
inline constexpr std::u8string_view comma = u8"，";
inline constexpr std::u8string_view person_close_ship_open = u8"）船（";
inline constexpr std::u8string_view close = u8"）";
inline constexpr std::u8string_view wei_xiaobao_location_prefix = u8"韋小寶：";
inline constexpr std::u8string_view world_minimap_title = u8"世界地圖";
inline constexpr std::u8string_view hud_poison_tag = u8"毒";
inline constexpr std::u8string_view hud_hurt_tag = u8"傷";
inline constexpr std::u8string_view hud_life_short = u8"生 ";
inline constexpr std::u8string_view hud_mp_short = u8"內 ";
inline constexpr std::u8string_view hud_power_short = u8"體 ";
inline constexpr std::u8string_view hud_magic_header = u8"武功";
inline constexpr std::u8string_view hud_level_header = u8"等級";
inline constexpr std::u8string_view hud_power_header = u8"威力";
inline constexpr std::u8string_view hud_cost_header = u8"內力";
inline constexpr std::u8string_view hud_level_suffix = u8"級";
inline constexpr std::u8string_view world_minimap_person = u8"人";
inline constexpr std::u8string_view world_minimap_ship = u8"船";
inline constexpr std::u8string_view world_minimap_teleport_prefix = u8"Enter傳送：";
inline constexpr std::u8string_view world_minimap_teleport_world_only = u8"傳送僅限大地圖";
inline constexpr std::u8string_view world_minimap_close_hint = u8"+/-縮放　方向鍵移動　ESC或地圖鍵關閉";
inline constexpr std::u8string_view open_parenthesis = u8"(";
inline constexpr std::u8string_view close_parenthesis = u8")";
inline constexpr std::u8string_view count_marker = u8"X";

inline constexpr std::array<std::u8string_view, 6> main_labels{
    u8"醫療",
    u8"解毒",
    u8"物品",
    u8"狀態",
    u8"離隊",
    u8"系統",
};
inline constexpr std::array<std::u8string_view, 3> system_labels{
    u8"讀檔",
    u8"存檔",
    u8"離開",
};
inline constexpr std::array<std::u8string_view, 3> slot_labels{
    u8"一",
    u8"二",
    u8"三",
};
inline constexpr std::u8string_view load_list_title = u8"讀取存檔";
inline constexpr std::u8string_view save_list_title = u8"儲存存檔";
inline constexpr std::u8string_view slot_number_header = u8"編號";
inline constexpr std::u8string_view name_header = u8"姓名";
inline constexpr std::u8string_view level_header = u8"LV";
inline constexpr std::u8string_view location_header = u8"地點";
inline constexpr std::u8string_view saved_at_header = u8"存檔時間";
inline constexpr std::u8string_view playthrough_header = u8"周目";
inline constexpr std::u8string_view empty_save_label = u8"空白";
inline constexpr std::u8string_view damaged_save_label = u8"損壞";
inline constexpr std::u8string_view name_prompt = u8"請輸入姓名  :";
inline constexpr std::u8string_view zhuyin_prompt = u8"（注音）：";
inline constexpr std::u8string_view alphanumeric_prompt = u8"（英數）：";
inline constexpr std::u8string_view modern_name_input_prompt = u8"（現代輸入）：";
inline constexpr std::u8string_view name_input_method_hint = u8"F12：切換輸入";
inline constexpr std::u8string_view no_name_candidates = u8"無此字";
inline constexpr std::u8string_view candidate_both_pages = u8"＜／＞";
inline constexpr std::u8string_view candidate_previous_page = u8"＜";
inline constexpr std::u8string_view candidate_next_page = u8"＞";
inline constexpr std::u8string_view attribute_question =
    u8"   這樣的屬性滿意嗎？（Ｙ／Ｎ）";
inline constexpr std::u8string_view quit_prompt = u8"真要離開遊戲（Ｙ／Ｎ）";
inline constexpr std::u8string_view delete_save_prompt = u8"刪除存檔";
inline constexpr std::u8string_view yes_no_prompt = u8"（Ｙ／Ｎ）";
inline constexpr std::u8string_view io_wait_label = u8"請稍候！";
inline constexpr std::u8string_view leave_protagonist_notice =
    u8"抱歉！沒有你遊戲進行不下去";
inline constexpr std::u8string_view equipment_unsuitable_notice =
    u8"此人不適合配備此物品";
inline constexpr std::u8string_view practice_assigned_notice =
    u8"此物品現在已經有人修練了";
inline constexpr std::u8string_view practice_reassign_question =
    u8"是否要換人修練（Ｙ／Ｎ）";
inline constexpr std::u8string_view practice_magic_full_notice =
    u8"一人只能修練十種功夫";
inline constexpr std::u8string_view practice_castration_notice =
    u8"修練此書必須先行揮劍自宮";
inline constexpr std::u8string_view practice_castration_question =
    u8"你是否仍要修練（Ｙ／Ｎ）";
inline constexpr std::u8string_view practice_unsuitable_notice =
    u8"此人不適合修練此物品";
inline constexpr std::array<std::u8string_view, 17> random_attribute_labels{
    u8"內力：",
    u8"武力：",
    u8"輕功：",
    u8"防禦：",
    u8"生命：",
    u8"醫療：",
    u8"使毒：",
    u8"解毒：",
    u8"拳掌：",
    u8"劍術：",
    u8"刀術：",
    u8"暗器：",
    u8"內力門路：",
    u8"生命成長：",
    u8"資質：",
    u8"抗毒能力：",
    u8"特殊兵器：",
};
inline constexpr std::array<std::u8string_view, 3> mp_type_labels{
    u8"陰",
    u8"陽",
    u8"陰陽合一",
};
inline constexpr std::u8string_view sexual_attribute_label = u8"性別：";
inline constexpr std::array<std::u8string_view, 3> sexual_labels{
    u8"男",
    u8"女",
    u8"不男不女",
};

inline constexpr std::u8string_view level_label = u8"等級 ";
inline constexpr std::u8string_view power_label = u8"体力 ";
inline constexpr std::u8string_view life_label = u8"生命 ";
inline constexpr std::u8string_view mp_label = u8"內力 ";
inline constexpr std::u8string_view experience_label = u8"經驗 ";
inline constexpr std::u8string_view upgrade_label = u8"升級 ";
inline constexpr std::u8string_view attack_label = u8"攻擊力 ";
inline constexpr std::u8string_view defence_label = u8"防禦力 ";
inline constexpr std::u8string_view speed_label = u8"輕功 ";
inline constexpr std::u8string_view medicine_label = u8"醫療能力 ";
inline constexpr std::u8string_view use_poison_label = u8"用毒能力 ";
inline constexpr std::u8string_view detox_label = u8"解毒能力 ";
inline constexpr std::u8string_view fist_label = u8"拳掌功夫 ";
inline constexpr std::u8string_view sword_label = u8"御劍能力 ";
inline constexpr std::u8string_view knife_label = u8"耍刀技巧 ";
inline constexpr std::u8string_view unusual_label = u8"特殊兵器 ";
inline constexpr std::u8string_view hidden_label = u8"暗器技巧 ";
inline constexpr std::u8string_view equipment_label = u8"裝備物品 ";
inline constexpr std::u8string_view practice_label = u8"修練物品 ";
inline constexpr std::u8string_view magic_label = u8"所會功夫 ";
inline constexpr std::u8string_view slash = u8"/";
inline constexpr std::u8string_view hundred = u8"100";
inline constexpr std::u8string_view maximum_level = u8"   =   ";
inline constexpr std::u8string_view maximum_practice = u8" = ";
inline constexpr std::u8string_view medicine_target_title = u8"要醫治誰";
inline constexpr std::u8string_view detoxification_target_title = u8"替誰解毒";
inline constexpr std::u8string_view status_selection_title = u8"要查閱誰的狀態";
inline constexpr std::u8string_view status_side_title = u8"要查閱哪方的狀態";
inline constexpr std::array<std::u8string_view, 2> status_side_labels{
    u8"我方",
    u8"敵方",
};
inline constexpr std::u8string_view equipment_target_title = u8"誰要配備";
inline constexpr std::u8string_view practice_target_title = u8"誰要修練";
inline constexpr std::u8string_view item_target_title = u8"誰要使用";
inline constexpr std::u8string_view leave_party_title = u8"要求誰離隊";
inline constexpr std::u8string_view life_points_label = u8"生命點數";
inline constexpr std::u8string_view poison_level_label = u8"中毒程度";
inline constexpr std::array<std::u8string_view, 3> status_tab_labels{
    u8"狀態",
    u8"能力",
    u8"武功",
};
inline constexpr std::u8string_view hurt_status_label = u8"內傷";
inline constexpr std::u8string_view poison_status_label = u8"中毒";
inline constexpr std::u8string_view morality_status_label = u8"道德";
inline constexpr std::u8string_view fame_status_label = u8"聲望";
inline constexpr std::u8string_view anti_poison_ability_label = u8"抗毒能力";
inline constexpr std::u8string_view mp_type_ability_label = u8"內力門路";
inline constexpr std::u8string_view increased_life_ability_label = u8"生命成長";
inline constexpr std::u8string_view iq_ability_label = u8"資質";
inline constexpr std::u8string_view attack_with_poison_ability_label = u8"功夫帶毒";
inline constexpr std::u8string_view attack_twice_ability_label = u8"左右互搏";
inline constexpr std::u8string_view ability_present_value = u8"有";
inline constexpr std::u8string_view sex_ability_label = u8"性別";
inline constexpr std::u8string_view knowledge_ability_label = u8"武學常識";
inline constexpr std::u8string_view status_level_suffix = u8"級";
inline constexpr std::u8string_view practice_experience_label = u8"修練經驗";
inline constexpr std::u8string_view practice_required_label = u8"所需經驗";
inline constexpr std::u8string_view magic_name_header = u8"武功";
inline constexpr std::u8string_view magic_level_header = u8"等級";
inline constexpr std::u8string_view magic_proficiency_header = u8"熟練度";
inline constexpr std::u8string_view magic_power_header = u8"威力";
inline constexpr std::u8string_view magic_poison_header = u8"帶毒";
inline constexpr std::u8string_view hurt_band_light_suffix = u8"(輕傷)";
inline constexpr std::u8string_view hurt_band_moderate_suffix = u8"(中傷)";
inline constexpr std::u8string_view hurt_band_severe_suffix = u8"(重傷)";
inline constexpr std::u8string_view unknown_value = u8"???";
inline constexpr std::array<std::u8string_view, 8> item_category_labels{
    u8"全部",
    u8"藥品",
    u8"武器",
    u8"防具",
    u8"秘笈",
    u8"暗器",
    u8"其他",
    u8"天書",
};
inline constexpr std::u8string_view item_count_prefix = u8"×";
inline constexpr std::u8string_view item_detail_craft_label = u8"煉製";
inline constexpr std::u8string_view item_detail_craft_experience_label = u8"煉製經驗";
inline constexpr std::u8string_view item_detail_material_label = u8"材料";
inline constexpr std::u8string_view item_detail_effect_label = u8"效果";
inline constexpr std::u8string_view item_detail_requirement_label = u8"需求";
inline constexpr std::u8string_view item_detail_learn_label = u8"習得";
inline constexpr std::u8string_view item_detail_only_label = u8"限";
inline constexpr std::u8string_view item_detail_none = u8"無";
inline constexpr std::u8string_view item_detail_at_least_suffix = u8"以上";
inline constexpr std::u8string_view item_detail_at_most_suffix = u8"以下";
inline constexpr std::u8string_view item_list_empty = u8"沒有物品";
inline constexpr std::u8string_view protagonist_location_label = u8"人";
inline constexpr std::u8string_view ship_location_label = u8"船";
inline constexpr std::u8string_view use_item_prefix = u8"使用 ";
inline constexpr std::u8string_view item_increase = u8"提升";
inline constexpr std::u8string_view item_decrease = u8"減少";
inline constexpr std::u8string_view item_mp_type_changed = u8"內力門路改為  陰陽合一";
inline constexpr std::array<std::u8string_view, 23> item_effect_labels{
    u8"生命值",
    u8"生命最大值",
    u8"中毒程度",
    u8"体力值",
    u8"內力門路",
    u8"內力值",
    u8"內力最大值",
    u8"武力值",
    u8"輕功值",
    u8"防禦力",
    u8"醫療能力",
    u8"使毒能力",
    u8"解毒能力",
    u8"抗毒能力",
    u8"拳掌功夫",
    u8"御劍能力",
    u8"耍刀技巧",
    u8"特殊兵器",
    u8"暗器技巧",
    u8"武學常識",
    u8"人性",
    u8"攻擊次數",
    u8"功夫帶毒",
};
inline constexpr std::u8string_view no_medicine_user = u8"隊員中無人醫術夠格";
inline constexpr std::u8string_view medicine_user_title = u8"誰要使用醫術";
inline constexpr std::u8string_view medicine_ability_label = u8"醫療能力";
inline constexpr std::u8string_view medicine_result_label = u8"恢復生命";
inline constexpr std::u8string_view no_detoxification_user = u8"隊員中無人解毒夠格";
inline constexpr std::u8string_view detoxification_user_title = u8"誰要幫人解毒";
inline constexpr std::u8string_view detoxification_ability_label = u8"解毒能力";
inline constexpr std::u8string_view detoxification_result_label = u8"幫助解毒";
inline constexpr std::u8string_view hurt_healed_result_label = u8"治癒內傷";

inline constexpr std::u8string_view party_selection_title = u8"請選擇參與戰鬥之人物";
inline constexpr std::u8string_view selected_marker = u8"*";
inline constexpr std::u8string_view confirm_label = u8"結束";
inline constexpr std::u8string_view attack_direction_prompt = u8"選擇攻擊方向";
inline constexpr std::array<std::u8string_view, 10> player_action_labels{
    u8"移動",
    u8"攻擊",
    u8"用毒",
    u8"解毒",
    u8"醫療",
    u8"物品",
    u8"等待",
    u8"狀態",
    u8"休息",
    u8"自動",
};
inline constexpr std::u8string_view battle_defeat = u8"戰鬥失敗";
inline constexpr std::u8string_view battle_victory = u8"戰鬥勝利";
inline constexpr std::u8string_view experience_gained = u8" 獲得經驗點數";
inline constexpr std::u8string_view level_up = u8" 升級了";
// 战后升级面板逐项列出的属性（与升级会改动的字段一一对应）。
inline constexpr std::array<std::u8string_view, 13> level_up_stat_labels{
    u8"等級", u8"生命", u8"內力", u8"攻擊力", u8"防禦力", u8"輕功", u8"醫療能力",
    u8"用毒能力", u8"解毒能力", u8"拳掌功夫", u8"御劍能力", u8"耍刀技巧", u8"暗器技巧",
};
inline constexpr std::u8string_view level_up_arrow = u8"==>";
inline constexpr std::u8string_view level_up_colon = u8"：";
inline constexpr std::u8string_view learned_magic_text = u8"學到新武功";
inline constexpr std::u8string_view learned_magic_open = u8"【";
inline constexpr std::u8string_view learned_magic_close = u8"】";
inline constexpr std::u8string_view practice_prefix = u8" 修練 ";
inline constexpr std::u8string_view practice_suffix = u8" 成功 ";
inline constexpr std::u8string_view magic_level_prefix = u8" 升級了 ";
inline constexpr std::u8string_view magic_level_suffix = u8" 級";
inline constexpr std::u8string_view crafted_item = u8" 製造出 ";
inline constexpr std::u8string_view level_prefix = u8" 升為第 ";
inline constexpr std::u8string_view level_suffix = u8" 級";

inline constexpr std::array<std::u8string_view, 4> progress_menu_items{
    u8"載入進度一",
    u8"載入進度二",
    u8"載入進度三",
    u8"離開睡覺去",
};
inline constexpr std::array<std::u8string_view, 2> death_menu_items{
    u8"載入進度",
    progress_menu_items[3],
};
inline constexpr std::u8string_view death_location = u8"在地球的某處";
inline constexpr std::u8string_view death_missing = u8"當地人口的失蹤數";
inline constexpr std::u8string_view death_another = u8"又多了一筆．．．";
inline constexpr std::u8string_view exit_prompt = u8"真要離開遊戲（Ｙ／Ｎ）";
inline constexpr std::u8string_view battle_question = u8"是否與之過招（Ｙ／Ｎ）";
inline constexpr std::u8string_view join_question = u8"是否要求加入（Ｙ／Ｎ）";
inline constexpr std::u8string_view rest_question = u8"是否住宿過夜（Ｙ／Ｎ）";
inline constexpr std::u8string_view item_notice_prefix = u8"得到";
inline constexpr std::u8string_view learn_magic_notice_infix = u8" 學會 ";
inline constexpr std::u8string_view role_iq_notice_infix = u8" 資質增加 ";
inline constexpr std::u8string_view role_speed_notice_infix = u8" 輕功增加 ";
inline constexpr std::u8string_view role_mp_notice_infix = u8" 內力增加 ";
inline constexpr std::u8string_view role_attack_notice_infix = u8" 武力增加 ";
inline constexpr std::u8string_view role_hp_notice_infix = u8" 生命增加 ";
inline constexpr std::u8string_view morality_notice_prefix = u8"你現在的品德指數為";
inline constexpr std::u8string_view fame_notice_prefix = u8"你現在個人聲望指數為";

inline constexpr auto kUnknown = unknown;
inline constexpr auto kPersonOpen = person_open;
inline constexpr auto kComma = comma;
inline constexpr auto kPersonCloseShipOpen = person_close_ship_open;
inline constexpr auto kClose = close;
inline constexpr auto kWeiXiaobaoLocationPrefix = wei_xiaobao_location_prefix;
inline constexpr auto kWorldMinimapTitle = world_minimap_title;
inline constexpr auto kHudPoisonTag = hud_poison_tag;
inline constexpr auto kHudHurtTag = hud_hurt_tag;
inline constexpr auto kHudLifeShort = hud_life_short;
inline constexpr auto kHudMpShort = hud_mp_short;
inline constexpr auto kHudPowerShort = hud_power_short;
inline constexpr auto kHudMagicHeader = hud_magic_header;
inline constexpr auto kHudLevelHeader = hud_level_header;
inline constexpr auto kHudPowerHeader = hud_power_header;
inline constexpr auto kHudCostHeader = hud_cost_header;
inline constexpr auto kHudLevelSuffix = hud_level_suffix;
inline constexpr auto kWorldMinimapPerson = world_minimap_person;
inline constexpr auto kWorldMinimapShip = world_minimap_ship;
inline constexpr auto kWorldMinimapCloseHint = world_minimap_close_hint;
inline constexpr auto kWorldMinimapTeleportPrefix = world_minimap_teleport_prefix;
inline constexpr auto kWorldMinimapTeleportWorldOnly = world_minimap_teleport_world_only;
inline constexpr auto kOpenParenthesis = open_parenthesis;
inline constexpr auto kCloseParenthesis = close_parenthesis;
inline constexpr auto kCountMarker = count_marker;
inline constexpr auto& kMainLabels = main_labels;
inline constexpr auto& kSystemLabels = system_labels;
inline constexpr auto& kSlotLabels = slot_labels;
inline constexpr auto kLoadListTitle = load_list_title;
inline constexpr auto kSaveListTitle = save_list_title;
inline constexpr auto kSlotNumberHeader = slot_number_header;
inline constexpr auto kNameHeader = name_header;
inline constexpr auto kLevelHeader = level_header;
inline constexpr auto kLocationHeader = location_header;
inline constexpr auto kSavedAtHeader = saved_at_header;
inline constexpr auto kPlaythroughHeader = playthrough_header;
inline constexpr auto kEmptySaveLabel = empty_save_label;
inline constexpr auto kDamagedSaveLabel = damaged_save_label;
inline constexpr auto kNamePrompt = name_prompt;
inline constexpr auto kZhuyinPrompt = zhuyin_prompt;
inline constexpr auto kAlnumPrompt = alphanumeric_prompt;
inline constexpr auto kModernNameInputPrompt = modern_name_input_prompt;
inline constexpr auto kNameInputMethodHint = name_input_method_hint;
inline constexpr auto kNoNameCandidates = no_name_candidates;
inline constexpr auto kCandidateBothPages = candidate_both_pages;
inline constexpr auto kCandidatePreviousPage = candidate_previous_page;
inline constexpr auto kCandidateNextPage = candidate_next_page;
inline constexpr auto kAttributeQuestion = attribute_question;
inline constexpr auto kQuitPrompt = quit_prompt;
inline constexpr auto kDeleteSavePrompt = delete_save_prompt;
inline constexpr auto kYesNoPrompt = yes_no_prompt;
inline constexpr auto kIoWaitLabel = io_wait_label;
inline constexpr auto kLeaveProtagonistNotice = leave_protagonist_notice;
inline constexpr auto kEquipmentUnsuitableNotice = equipment_unsuitable_notice;
inline constexpr auto kPracticeAssignedNotice = practice_assigned_notice;
inline constexpr auto kPracticeReassignQuestion = practice_reassign_question;
inline constexpr auto kPracticeMagicFullNotice = practice_magic_full_notice;
inline constexpr auto kPracticeCastrationNotice = practice_castration_notice;
inline constexpr auto kPracticeCastrationQuestion = practice_castration_question;
inline constexpr auto kPracticeUnsuitableNotice = practice_unsuitable_notice;
inline constexpr auto& kRandomAttributeLabels = random_attribute_labels;
inline constexpr auto& kMpTypeLabels = mp_type_labels;
inline constexpr auto kSexualAttributeLabel = sexual_attribute_label;
inline constexpr auto& kSexualLabels = sexual_labels;
inline constexpr auto kLevelLabel = level_label;
inline constexpr auto kPowerLabel = power_label;
inline constexpr auto kLifeLabel = life_label;
inline constexpr auto kMpLabel = mp_label;
inline constexpr auto kExperienceLabel = experience_label;
inline constexpr auto kUpgradeLabel = upgrade_label;
inline constexpr auto kAttackLabel = attack_label;
inline constexpr auto kDefenceLabel = defence_label;
inline constexpr auto kSpeedLabel = speed_label;
inline constexpr auto kMedicineLabel = medicine_label;
inline constexpr auto kUsePoisonLabel = use_poison_label;
inline constexpr auto kDetoxLabel = detox_label;
inline constexpr auto kFistLabel = fist_label;
inline constexpr auto kSwordLabel = sword_label;
inline constexpr auto kKnifeLabel = knife_label;
inline constexpr auto kUnusualLabel = unusual_label;
inline constexpr auto kHiddenLabel = hidden_label;
inline constexpr auto kEquipmentLabel = equipment_label;
inline constexpr auto kPracticeLabel = practice_label;
inline constexpr auto kMagicLabel = magic_label;
inline constexpr auto kSlash = slash;
inline constexpr auto kHundred = hundred;
inline constexpr auto kMaximumLevel = maximum_level;
inline constexpr auto kMaximumPractice = maximum_practice;
inline constexpr auto kMedicineTargetTitle = medicine_target_title;
inline constexpr auto kDetoxificationTargetTitle = detoxification_target_title;
inline constexpr auto kStatusSelectionTitle = status_selection_title;
inline constexpr auto kStatusSideTitle = status_side_title;
inline constexpr auto& kStatusSideLabels = status_side_labels;
inline constexpr auto kEquipmentTargetTitle = equipment_target_title;
inline constexpr auto kPracticeTargetTitle = practice_target_title;
inline constexpr auto kItemTargetTitle = item_target_title;
inline constexpr auto kLeavePartyTitle = leave_party_title;
inline constexpr auto kLifePointsLabel = life_points_label;
inline constexpr auto kPoisonLevelLabel = poison_level_label;
inline constexpr auto& kStatusTabLabels = status_tab_labels;
inline constexpr auto kHurtStatusLabel = hurt_status_label;
inline constexpr auto kPoisonStatusLabel = poison_status_label;
inline constexpr auto kMoralityStatusLabel = morality_status_label;
inline constexpr auto kFameStatusLabel = fame_status_label;
inline constexpr auto kAntiPoisonAbilityLabel = anti_poison_ability_label;
inline constexpr auto kMpTypeAbilityLabel = mp_type_ability_label;
inline constexpr auto kIncreasedLifeAbilityLabel = increased_life_ability_label;
inline constexpr auto kIqAbilityLabel = iq_ability_label;
inline constexpr auto kAttackWithPoisonAbilityLabel = attack_with_poison_ability_label;
inline constexpr auto kAttackTwiceAbilityLabel = attack_twice_ability_label;
inline constexpr auto kAbilityPresentValue = ability_present_value;
inline constexpr auto kSexAbilityLabel = sex_ability_label;
inline constexpr auto kKnowledgeAbilityLabel = knowledge_ability_label;
inline constexpr auto kStatusLevelSuffix = status_level_suffix;
inline constexpr auto kPracticeExperienceLabel = practice_experience_label;
inline constexpr auto kPracticeRequiredLabel = practice_required_label;
inline constexpr auto kMagicNameHeader = magic_name_header;
inline constexpr auto kMagicLevelHeader = magic_level_header;
inline constexpr auto kMagicProficiencyHeader = magic_proficiency_header;
inline constexpr auto kMagicPowerHeader = magic_power_header;
inline constexpr auto kMagicPoisonHeader = magic_poison_header;
inline constexpr auto kHurtBandLightSuffix = hurt_band_light_suffix;
inline constexpr auto kHurtBandModerateSuffix = hurt_band_moderate_suffix;
inline constexpr auto kHurtBandSevereSuffix = hurt_band_severe_suffix;
inline constexpr auto kUnknownValue = unknown_value;
inline constexpr auto& kItemCategoryLabels = item_category_labels;
inline constexpr auto kItemCountPrefix = item_count_prefix;
inline constexpr auto kItemDetailCraftLabel = item_detail_craft_label;
inline constexpr auto kItemDetailCraftExperienceLabel = item_detail_craft_experience_label;
inline constexpr auto kItemDetailMaterialLabel = item_detail_material_label;
inline constexpr auto kItemDetailEffectLabel = item_detail_effect_label;
inline constexpr auto kItemDetailRequirementLabel = item_detail_requirement_label;
inline constexpr auto kItemDetailLearnLabel = item_detail_learn_label;
inline constexpr auto kItemDetailOnlyLabel = item_detail_only_label;
inline constexpr auto kItemDetailNone = item_detail_none;
inline constexpr auto kItemDetailAtLeastSuffix = item_detail_at_least_suffix;
inline constexpr auto kItemDetailAtMostSuffix = item_detail_at_most_suffix;
inline constexpr auto kItemListEmpty = item_list_empty;
inline constexpr auto kProtagonistLocationLabel = protagonist_location_label;
inline constexpr auto kShipLocationLabel = ship_location_label;
inline constexpr auto kUseItemPrefix = use_item_prefix;
inline constexpr auto kItemIncrease = item_increase;
inline constexpr auto kItemDecrease = item_decrease;
inline constexpr auto kItemMpTypeChanged = item_mp_type_changed;
inline constexpr auto& kItemEffectLabels = item_effect_labels;
inline constexpr auto kNoMedicineUser = no_medicine_user;
inline constexpr auto kMedicineUserTitle = medicine_user_title;
inline constexpr auto kMedicineAbilityLabel = medicine_ability_label;
inline constexpr auto kMedicineResultLabel = medicine_result_label;
inline constexpr auto kNoDetoxificationUser = no_detoxification_user;
inline constexpr auto kDetoxificationUserTitle = detoxification_user_title;
inline constexpr auto kDetoxificationAbilityLabel = detoxification_ability_label;
inline constexpr auto kDetoxificationResultLabel = detoxification_result_label;
inline constexpr auto kHurtHealedResultLabel = hurt_healed_result_label;
inline constexpr auto kPartySelectionTitle = party_selection_title;
inline constexpr auto kSelectedMarker = selected_marker;
inline constexpr auto kConfirmLabel = confirm_label;
inline constexpr auto kAttackDirectionPrompt = attack_direction_prompt;
inline constexpr auto& kPlayerActionLabels = player_action_labels;
inline constexpr auto kBattleDefeatText = battle_defeat;
inline constexpr auto kBattleVictoryText = battle_victory;
inline constexpr auto kExperienceGainedText = experience_gained;
inline constexpr auto kLevelUpText = level_up;
inline constexpr auto kLevelUpStatLabels = level_up_stat_labels;
inline constexpr auto kLevelUpArrow = level_up_arrow;
inline constexpr auto kLevelUpColon = level_up_colon;
inline constexpr auto kLearnedMagicText = learned_magic_text;
inline constexpr auto kLearnedMagicOpen = learned_magic_open;
inline constexpr auto kLearnedMagicClose = learned_magic_close;
inline constexpr auto kPracticePrefix = practice_prefix;
inline constexpr auto kPracticeSuffix = practice_suffix;
inline constexpr auto kMagicLevelPrefix = magic_level_prefix;
inline constexpr auto kMagicLevelSuffix = magic_level_suffix;
inline constexpr auto kCraftedItemText = crafted_item;
inline constexpr auto kLevelPrefix = level_prefix;
inline constexpr auto kLevelSuffix = level_suffix;
inline constexpr auto& kProgressMenuItems = progress_menu_items;
inline constexpr auto& kDeathMenuItems = death_menu_items;
inline constexpr auto kDeathLocationText = death_location;
inline constexpr auto kDeathMissingText = death_missing;
inline constexpr auto kDeathAnotherText = death_another;
inline constexpr auto kExitPrompt = exit_prompt;
inline constexpr auto kBattleQuestion = battle_question;
inline constexpr auto kJoinQuestion = join_question;
inline constexpr auto kRestQuestion = rest_question;
inline constexpr auto kItemNoticePrefix = item_notice_prefix;
inline constexpr auto kLearnMagicNoticeInfix = learn_magic_notice_infix;
inline constexpr auto kRoleIqNoticeInfix = role_iq_notice_infix;
inline constexpr auto kRoleSpeedNoticeInfix = role_speed_notice_infix;
inline constexpr auto kRoleMpNoticeInfix = role_mp_notice_infix;
inline constexpr auto kRoleAttackNoticeInfix = role_attack_notice_infix;
inline constexpr auto kRoleHpNoticeInfix = role_hp_notice_infix;
inline constexpr auto kMoralityNoticePrefix = morality_notice_prefix;
inline constexpr auto kFameNoticePrefix = fame_notice_prefix;

}  // namespace openlegend::text::game_strings
