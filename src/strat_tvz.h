
struct strat_tvz {


	void run() {

		combat::no_aggressive_groups = true;

		get_upgrades::set_upgrade_value(upgrade_types::terran_vehicle_weapons_1, 2000.0);

		bool built_missile_turret = false;
		while (true) {

			using namespace buildpred;

			int my_tank_count = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size();
			int my_goliath_count = my_units_of_type[unit_types::goliath].size();
			int my_marine_count = my_units_of_type[unit_types::marine].size();
			int my_science_vessel_count = my_units_of_type[unit_types::science_vessel].size();
			int my_wraith_count = my_units_of_type[unit_types::wraith].size();
			int my_valkyrie_count = my_units_of_type[unit_types::valkyrie].size();

			int enemy_mutalisk_count = 0;
			int enemy_guardian_count = 0;
			int enemy_lurker_count = 0;
			int enemy_hydralisk_count = 0;
			int enemy_hydralisk_den_count = 0;
			int enemy_lair_count = 0;
			int enemy_spire_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::mutalisk) ++enemy_mutalisk_count;
				if (e->type == unit_types::guardian || e->type == unit_types::cocoon) ++enemy_guardian_count;
				if (e->type == unit_types::lurker || e->type == unit_types::lurker_egg) ++enemy_lurker_count;
				if (e->type == unit_types::hydralisk) ++enemy_hydralisk_count;
				if (e->type == unit_types::hydralisk_den) ++enemy_hydralisk_den_count;
				if (e->type == unit_types::lair) ++enemy_lair_count;
				if (e->type == unit_types::spire || e->type==unit_types::greater_spire) ++enemy_spire_count;
			}

			if (my_tank_count + my_goliath_count / 2 + my_marine_count / 4 >= 8) combat::no_aggressive_groups = false;
			if (my_tank_count + my_goliath_count / 2 + my_marine_count / 4 < 4) combat::no_aggressive_groups = true;
			if (enemy_lurker_count >= my_tank_count && my_science_vessel_count == 0) combat::no_aggressive_groups = true;
			if (enemy_lurker_count >= 4 && my_tank_count < 4) combat::no_aggressive_groups = true;

			if (my_tank_count < 2 && enemy_mutalisk_count == 0 && enemy_spire_count == 0) {
				if ((enemy_hydralisk_den_count != 0) != (enemy_lair_count != 0)) {
					// TODO: instead of doing this, the wraith should be added as a scout with a mission to scout buildings,
					//       or something like that
					unit*scout_building = get_best_score(enemy_buildings, [&](unit*u) {
						int age = current_frame - u->last_seen;
						if (age < 15 * 60) return std::numeric_limits<double>::infinity();
						return (double)age;
					}, std::numeric_limits<double>::infinity());
					if (scout_building) {
						for (auto*c : combat::live_combat_units) {
							if (c->u->type == unit_types::wraith) {
								c->strategy_busy_until = current_frame + 15 * 8;
								c->action = combat::combat_unit::action_offence;
								c->subaction = combat::combat_unit::subaction_move;
								c->target_pos = scout_building->pos;
								break;
							}
						}
					}
				}
			}

			bool lurkers_are_coming = my_tank_count <= 2 && enemy_mutalisk_count == 0 && (enemy_lurker_count || (enemy_hydralisk_den_count && enemy_lair_count));
			if (lurkers_are_coming) {
				log("lurkers are coming!\n");
				get_upgrades::set_no_auto_upgrades(true);
				if (!my_units_of_type[unit_types::science_facility].empty()) get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
			} else {
				get_upgrades::set_no_auto_upgrades(false);
				if (my_tank_count >= 1) get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1);
			}

			int desired_science_vessel_count = (enemy_lurker_count + 3) / 4;
			if (desired_science_vessel_count == 0 && lurkers_are_coming) ++desired_science_vessel_count;
			if (desired_science_vessel_count > 1 && my_tank_count + my_goliath_count < 4) desired_science_vessel_count = 1;
			int desired_goliath_count = 2 + enemy_mutalisk_count / 2 + enemy_mutalisk_count / 3 + enemy_guardian_count * 2;
			if (my_marine_count < 10 && my_goliath_count < 2 && my_tank_count < 3) desired_goliath_count = 0;
			int desired_wraith_count = 1 + (my_tank_count + my_goliath_count) / 8;
			if (my_tank_count >= 4 && enemy_mutalisk_count + enemy_hydralisk_count < 6) desired_wraith_count += 2;
			if (my_goliath_count < 3 && my_valkyrie_count && enemy_mutalisk_count + enemy_spire_count) desired_wraith_count += 2;
			int desired_valkyrie_count = std::min(enemy_mutalisk_count / 4, 4);
			if ((enemy_spire_count || enemy_mutalisk_count) && my_goliath_count + my_wraith_count >= 3) desired_valkyrie_count += 2;
			if (my_tank_count >= 12) desired_valkyrie_count += 3;
			int desired_medic_count = my_marine_count / 8;
			if (my_tank_count < 3 && my_marine_count < 10) desired_medic_count = 0;

			combat::aggressive_wraiths = enemy_mutalisk_count <= my_wraith_count;

			if (lurkers_are_coming) {
				desired_goliath_count = 0;
				desired_wraith_count = 0;
				desired_valkyrie_count = 0;
				desired_medic_count = 0;
			}

			auto build = [&](state&st) {
				return nodelay(st, unit_types::scv, [&](state&st) {
					std::function<bool(state&)> army = [&](state&st) {
						if (lurkers_are_coming || my_tank_count < 1) {
							return maxprod(st, unit_types::siege_tank_tank_mode, [&](state&st) {
								return depbuild(st, state(st), unit_types::marine);
							});
						}
						return nodelay(st, unit_types::marine, [&](state&st) {
							if (st.gas < 100) {
								return maxprod(st, unit_types::vulture, [&](state&st) {
									return maxprod1(st, unit_types::siege_tank_tank_mode);
								});
							}
							return maxprod(st, unit_types::siege_tank_tank_mode, [&](state&st) {
								return maxprod1(st, unit_types::vulture);
							});
						});
					};
					if (my_tank_count >= 1) {
						int goliath_count = count_units_plus_production(st, unit_types::goliath);
						if (goliath_count < desired_goliath_count) {
							army = [army](state&st) {
								return maxprod(st, unit_types::goliath, army);
							};
						}
					}
					int wraith_count = count_units_plus_production(st, unit_types::wraith);
					if (wraith_count < desired_wraith_count) {
						army = [army](state&st) {
							return nodelay(st, unit_types::wraith, army);
						};
					}
					int valkyrie_count = count_units_plus_production(st, unit_types::valkyrie);
					if (valkyrie_count < desired_valkyrie_count) {
						army = [army](state&st) {
							return nodelay(st, unit_types::valkyrie, army);
						};
					}
					int science_vessel_count = count_units_plus_production(st, unit_types::science_vessel);
					if (science_vessel_count < desired_science_vessel_count) {
						army = [army](state&st) {
							return nodelay(st, unit_types::science_vessel, army);
						};
					}
					int medic_count = count_units_plus_production(st, unit_types::medic);
					if (medic_count < desired_medic_count) {
						army = [army](state&st) {
							return nodelay(st, unit_types::medic, army);
						};
					}
// 						if (count_units_plus_production(st, unit_types::missile_turret)) built_missile_turret = true;
// 						if (lurkers_are_coming && count_units_plus_production(st, unit_types::missile_turret) == 0 && !built_missile_turret) {
// 							army = [army](state&st) {
// 								return nodelay(st, unit_types::missile_turret, army);
// 							};
// 						}
					if (count_units_plus_production(st, unit_types::barracks) < 2) {
						return nodelay(st, unit_types::barracks, army);
					}

					return army(st);
				});
			};

			auto is_long_distance_mining = [&]() {
				int count = 0;
				for (auto&g : resource_gathering::live_gatherers) {
					if (!g.resource) continue;
					unit*ru = g.resource->u;
					resource_spots::spot*rs = nullptr;
					for (auto&s : resource_spots::spots) {
						if (grid::get_build_square(s.cc_build_pos).building) continue;
						for (auto&r : s.resources) {
							if (r.u == ru) {
								rs = &s;
								break;
							}
						}
						if (rs) break;
					}
					if (rs) ++count;
				}
				return count >= 8;
			};
			auto can_expand = [&]() {
				if (buildpred::get_my_current_state().bases.size() == 2 && combat::no_aggressive_groups) return false;
				return is_long_distance_mining();
			};

			execute_build(can_expand(), build);

			multitasking::sleep(15 * 5);
		}

	}

};
