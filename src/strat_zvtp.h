
struct strat_zvtp {

	void run() {

		using namespace buildpred;

		combat::no_aggressive_groups = false;
		get_upgrades::set_no_auto_upgrades(true);

		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_1, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_2, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_3, -1.0);

		get_upgrades::set_upgrade_value(upgrade_types::pneumatized_carapace, -1.0);

		get_upgrades::set_upgrade_value(upgrade_types::consume, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::plague, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::consume, -10.0);
		get_upgrades::set_upgrade_order(upgrade_types::plague, -9.0);

		get_upgrades::set_upgrade_order(upgrade_types::adrenal_glands, -20.0);

		auto place_static_defence = [&]() {
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					if (b.build_near != combat::defence_choke.center) {
						b.build_near = combat::defence_choke.center;
						build::unset_build_pos(&b);
					}
				}
			}
		};

		auto long_distance_miners = [&]() {
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
			return count;
		};

		auto get_next_base = [&]() {
			auto st = get_my_current_state();
			unit_type*worker_type = unit_types::drone;
			unit_type*cc_type = unit_types::hatchery;
			a_vector<refcounted_ptr<resource_spots::spot>> available_bases;
			for (auto&s : resource_spots::spots) {
				if (grid::get_build_square(s.cc_build_pos).building) continue;
				bool okay = true;
				for (auto&v : st.bases) {
					if ((resource_spots::spot*)v.s == &s) okay = false;
				}
				if (!okay) continue;
				if (!build::build_is_valid(grid::get_build_square(s.cc_build_pos), cc_type)) continue;
				available_bases.push_back(&s);
			}
			return get_best_score(available_bases, [&](resource_spots::spot*s) {
				double score = unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos);
				double res = 0;
				double ned = get_best_score_value(enemy_buildings, [&](unit*e) {
					if (e->type->is_worker) return std::numeric_limits<double>::infinity();
					return diag_distance(s->pos - e->pos);
				});
				if (ned <= 32 * 30) score += 10000;
				bool has_gas = false;
				for (auto&r : s->resources) {
					res += r.u->resources;
					if (r.u->type->is_gas) has_gas = true;
				}
				score -= (has_gas ? res : res / 4) / 10;
				return score;
			}, std::numeric_limits<double>::infinity());
		};

		auto place_expos = [&]() {

			auto st = get_my_current_state();
			int free_mineral_patches = 0;
			for (auto&b : st.bases) {
				for (auto&s : b.s->resources) {
					resource_gathering::resource_t*res = nullptr;
					for (auto&s2 : resource_gathering::live_resources) {
						if (s2.u == s.u) {
							res = &s2;
							break;
						}
					}
					if (res) {
						if (res->gatherers.empty()) ++free_mineral_patches;
					}
				}
			}
			if (free_mineral_patches >= 8 && long_distance_miners() == 0) return;

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::hatchery) {
					xy pos = b.build_pos;
					build::unset_build_pos(&b);
					auto s = get_next_base();
					if (s) pos = s->cc_build_pos;
					if (pos != xy()) build::set_build_pos(&b, pos);
					else build::unset_build_pos(&b);
				}
			}

		};

		bool is_attacking = false;
		double attack_my_lost;
		double attack_op_lost;
		int last_attack_begin = 0;
		int last_attack_end = 0;
		int attack_count = 0;

		auto begin_attack = [&]() {
			is_attacking = true;
			attack_my_lost = players::my_player->minerals_lost + players::my_player->gas_lost;
			attack_op_lost = players::opponent_player->minerals_lost + players::opponent_player->gas_lost;
			last_attack_begin = current_frame;
			++attack_count;
			log("begin attack\n");
		};
		auto end_attack = [&]() {
			is_attacking = false;
			last_attack_end = current_frame;
			log("end attack\n");
		};

		auto weapon_damage_against_size = [&](weapon_stats*w, int size) {
			if (!w) return 0.0;
			double damage = w->damage;
			damage *= combat_eval::get_damage_type_modifier(w->damage_type, size);
			if (damage <= 0) damage = 1.0;
			return damage;
		};
		
		bool maxed_out_aggression = false;
		while (true) {

			int my_zergling_count = my_units_of_type[unit_types::zergling].size();
			int my_queen_count = my_units_of_type[unit_types::queen].size();
			int my_defiler_count = my_units_of_type[unit_types::defiler].size();
			int my_mutalisk_count = my_units_of_type[unit_types::mutalisk].size();

			int my_hatch_count = my_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() + my_units_of_type[unit_types::hive].size();

			int enemy_marine_count = 0;
			int enemy_firebat_count = 0;
			int enemy_ghost_count = 0;
			int enemy_vulture_count = 0;
			int enemy_tank_count = 0;
			int enemy_goliath_count = 0;
			int enemy_wraith_count = 0;
			int enemy_valkyrie_count = 0;
			int enemy_bc_count = 0;
			int enemy_science_vessel_count = 0;
			int enemy_dropship_count = 0;
			double enemy_army_supply = 0.0;
			double enemy_air_army_supply = 0.0;
			double enemy_ground_army_supply = 0.0;
			double enemy_ground_large_army_supply = 0.0;
			double enemy_ground_small_army_supply = 0.0;
			double enemy_weak_against_ultra_supply = 0.0;
			int enemy_static_defence_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::marine) ++enemy_marine_count;
				if (e->type == unit_types::firebat) ++enemy_firebat_count;
				if (e->type == unit_types::ghost) ++enemy_ghost_count;
				if (e->type == unit_types::vulture) ++enemy_vulture_count;
				if (e->type == unit_types::siege_tank_tank_mode) ++enemy_tank_count;
				if (e->type == unit_types::siege_tank_siege_mode) ++enemy_tank_count;
				if (e->type == unit_types::goliath) ++enemy_goliath_count;
				if (e->type == unit_types::wraith) ++enemy_wraith_count;
				if (e->type == unit_types::valkyrie) ++enemy_valkyrie_count;
				if (e->type == unit_types::battlecruiser) ++enemy_bc_count;
				if (e->type == unit_types::science_vessel) ++enemy_science_vessel_count;
				if (e->type == unit_types::dropship) ++enemy_dropship_count;
				if (!e->type->is_worker && !e->building) {
					if (e->is_flying) enemy_air_army_supply += e->type->required_supply;
					else {
						enemy_ground_army_supply += e->type->required_supply;
						if (e->type->size == unit_type::size_large) enemy_ground_large_army_supply += e->type->required_supply;
						if (e->type->size == unit_type::size_small) enemy_ground_small_army_supply += e->type->required_supply;
						if (weapon_damage_against_size(e->stats->ground_weapon, unit_type::size_large) <= 12.0) enemy_weak_against_ultra_supply += e->type->required_supply;
					}
					enemy_army_supply += e->type->required_supply;
				}
				if (e->type == unit_types::missile_turret) ++enemy_static_defence_count;
				if (e->type == unit_types::photon_cannon) ++enemy_static_defence_count;
				if (e->type == unit_types::sunken_colony) ++enemy_static_defence_count;
				if (e->type == unit_types::spore_colony) ++enemy_static_defence_count;
			}
			
// 			double my_air_army_supply = 0.0;
// 			double my_ground_army_supply = 0.0;
// 			for (unit*u : my_units) {
// 				if (!u->type->is_worker) {
// 					if (u->is_flying)  my_air_army_supply += u->type->required_supply;
// 					else my_ground_army_supply += u->type->required_supply;
// 				}
// 			}

			if (!is_attacking) {

				bool attack = false;
				if (attack_count == 0) attack = true;
				if (current_frame - last_attack_end >= 15 * 60 * 2) attack = true;

				if (attack) begin_attack();

			} else {

				int lose_count = 0;
				int total_count = 0;
				for (auto*a : combat::live_combat_units) {
					if (a->u->type->is_worker) continue;
					++total_count;
					if (current_frame - a->last_fight <= 15 * 10) {
						if (a->last_win_ratio < 1.0) ++lose_count;
					}
				}
				if (lose_count >= total_count / 2) {
					end_attack();
				}
				
			}

			combat::no_aggressive_groups = !is_attacking;

			if (current_used_total_supply >= 190.0) maxed_out_aggression = true;
			if (maxed_out_aggression) {
				combat::no_aggressive_groups = false;
				if (current_used_total_supply < 150.0) maxed_out_aggression = false;
			}

			if (current_used_total_supply >= 100 && get_upgrades::no_auto_upgrades) {
				get_upgrades::set_no_auto_upgrades(false);
			}

			if (enemy_ground_small_army_supply >= 10) {
				get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
			} else {
				if (current_used_total_supply >= 100) get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
				else get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, 0.0);
			}

			if (enemy_ground_large_army_supply >= 10) {
				get_upgrades::set_upgrade_value(upgrade_types::muscular_augments, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::grooved_spines, -1.0);
				get_upgrades::set_upgrade_order(upgrade_types::lurker_aspect, -11.0);
				get_upgrades::set_upgrade_order(upgrade_types::muscular_augments, -10.0);
				get_upgrades::set_upgrade_order(upgrade_types::grooved_spines, -9.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_missile_attacks_1, -1.0);
			}
			if (my_queen_count) {
				if (enemy_ground_small_army_supply >= 20) get_upgrades::set_upgrade_value(upgrade_types::ensnare, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::spawn_broodling, -1.0);
				get_upgrades::set_upgrade_order(upgrade_types::ensnare, -10.0);
				get_upgrades::set_upgrade_order(upgrade_types::spawn_broodling, -9.0);
			}

			if (my_mutalisk_count >= 4) {
				get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_carapace_1, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_carapace_2, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_carapace_3, -1.0);
				if (my_mutalisk_count >= 10) {
					get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_attacks_1, -1.0);
					get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_attacks_2, -1.0);
					get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_attacks_3, -1.0);
				}
			}

			bool force_expand = long_distance_miners() >= 1 && get_next_base();

			auto build = [&](state&st) {
				int drone_count = count_units_plus_production(st, unit_types::drone);
				int hatch_count = count_units_plus_production(st, unit_types::hatchery) + count_units_plus_production(st, unit_types::lair) + count_units_plus_production(st, unit_types::hive);
				int larva_count = 0;
				for (int i = 0; i < 3; ++i) {
					for (st_unit&u : st.units[i == 0 ? unit_types::hive : i == 1 ? unit_types::lair : unit_types::hatchery]) {
						larva_count += u.larva_count;
					}
				}

				int zergling_count = count_units_plus_production(st, unit_types::zergling);
				int mutalisk_count = count_units_plus_production(st, unit_types::mutalisk);
				int scourge_count = count_units_plus_production(st, unit_types::scourge);
				int hydralisk_count = count_units_plus_production(st, unit_types::hydralisk);
				int lurker_count = count_units_plus_production(st, unit_types::lurker);
				int ultralisk_count = count_units_plus_production(st, unit_types::ultralisk);
				int queen_count = count_units_plus_production(st, unit_types::queen);
				int defiler_count = count_units_plus_production(st, unit_types::defiler);
				int devourer_count = count_units_plus_production(st, unit_types::devourer);
				int guardian_count = count_units_plus_production(st, unit_types::guardian);

				std::function<bool(state&)> army = [&](state&st) {
					return false;
				};

				if (larva_count < st.minerals / 50 && larva_count < 40) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}
				if (drone_count < 100) {
					army = [army](state&st) {
						return nodelay(st, unit_types::drone, army);
					};
				}

				if (drone_count >= 24) {
					if (count_units_plus_production(st, unit_types::evolution_chamber) == (drone_count >= 60 ? 3 : 1)) {
						army = [army](state&st) {
							return nodelay(st, unit_types::evolution_chamber, army);
						};
					}
				}

				double army_supply = 0.0;
				double air_army_supply = 0.0;
				double ground_army_supply = 0.0;
				for (auto&v : st.units) {
					unit_type*ut = v.first;
					if (!ut->is_worker) {
						army_supply += ut->required_supply;
						if (ut->is_flyer) air_army_supply += ut->required_supply;
						else ground_army_supply += ut->required_supply;
					}
				}
				for (auto&v : st.production) {
					unit_type*ut = v.second;
					if (!ut->is_worker) {
						army_supply += ut->required_supply;
						if (ut->is_flyer) air_army_supply += ut->required_supply;
						else ground_army_supply += ut->required_supply;
					}
				}

				if (air_army_supply < 24) {
					army = [army](state&st) {
						return nodelay(st, unit_types::mutalisk, army);
					};
					if (scourge_count < enemy_air_army_supply) {
						army = [army](state&st) {
							return nodelay(st, unit_types::scourge, army);
						};
					}
				}

				//double army_supply = st.used_supply[race_zerg] - drone_count;
				if (army_supply < enemy_army_supply*1.25 || drone_count >= 60) {
					if (ground_army_supply < enemy_ground_army_supply) {
						army = [army](state&st) {
							return nodelay(st, unit_types::zergling, army);
						};
					}
					army = [army](state&st) {
						return nodelay(st, unit_types::mutalisk, army);
					};
					if (hydralisk_count < mutalisk_count * 2 && ground_army_supply >= enemy_ground_army_supply) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk, army);
						};
					}

					if (enemy_air_army_supply > air_army_supply && ground_army_supply >= enemy_ground_army_supply) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk, army);
						};
					}
					if (scourge_count < enemy_air_army_supply) {
						army = [army](state&st) {
							return nodelay(st, unit_types::scourge, army);
						};
					}
					if (hydralisk_count < guardian_count * 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk, army);
						};
					}

					if (hydralisk_count < enemy_ground_large_army_supply) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk, army);
						};
					}
					if (players::my_player->has_upgrade(upgrade_types::lurker_aspect)) {
						if (lurker_count * 2 < enemy_ground_small_army_supply) {
							army = [army](state&st) {
								return nodelay(st, unit_types::lurker, army);
							};
						}
					}

					if (count_units_plus_production(st, unit_types::ultralisk_cavern)) {
						if (ultralisk_count * 4 < enemy_weak_against_ultra_supply) {
							army = [army](state&st) {
								return nodelay(st, unit_types::ultralisk, army);
							};
						}
					}

					if (count_units_plus_production(st, unit_types::defiler_mound)) {
						if (defiler_count < ground_army_supply / 14) {
							army = [army](state&st) {
								return nodelay(st, unit_types::defiler, army);
							};
						}
					}
					if (count_units_plus_production(st, unit_types::greater_spire)) {
						if (enemy_air_army_supply >= 20) {
							double mult = enemy_air_army_supply > enemy_ground_army_supply ? 0.5 : 0.25;
							if (devourer_count < (int)(mutalisk_count * mult)) {
								army = [army](state&st) {
									return nodelay(st, unit_types::devourer, army);
								};
							}
						}
						if (enemy_ground_army_supply >= 20) {
							double mult = enemy_ground_army_supply > enemy_air_army_supply ? 0.5 : 0.25;
							if (guardian_count < (int)(mutalisk_count * mult)) {
								army = [army](state&st) {
									return nodelay(st, unit_types::guardian, army);
								};
							}
						}
					}


					if (army_supply >= enemy_army_supply * 2) {
						if (ground_army_supply < air_army_supply) {
							army = [army](state&st) {
								return nodelay(st, unit_types::hydralisk, army);
							};
						} else {
							army = [army](state&st) {
								return nodelay(st, unit_types::mutalisk, army);
							};
						}
						if (guardian_count < enemy_static_defence_count) {
							army = [army](state&st) {
								return nodelay(st, unit_types::guardian, army);
							};
						}
					}

				}

				if (army_supply >= 24 || st.used_supply[race_zerg] >= 80) {

// 					if (army_supply >= enemy_army_supply && army_supply < enemy_army_supply * 2) {
// 						int count = std::max((int)(enemy_ground_army_supply / 2), 8);
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::queen, army);
// 						};
// 					}

					if (count_units_plus_production(st, unit_types::hive) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hive, army);
						};
					} else {
						if (count_units_plus_production(st, unit_types::greater_spire) == 0) {
							bool all_spires_are_busy = true;
							for (unit*u : my_completed_units_of_type[unit_types::spire]) {
								if (!u->remaining_upgrade_time) {
									all_spires_are_busy = false;
									break;
								}
							}
							army = [army](state&st) {
								return nodelay(st, unit_types::greater_spire, army);
							};
							if (all_spires_are_busy && count_units_plus_production(st, unit_types::spire) < 3) {
								army = [army](state&st) {
									return nodelay(st, unit_types::spire, army);
								};
							}
						}
						if (ground_army_supply >= 14 || army_supply >= 60) {
							if (count_units_plus_production(st, unit_types::defiler_mound) == 0) {
								army = [army](state&st) {
									return nodelay(st, unit_types::defiler_mound, army);
								};
							}
						}
						if (enemy_ground_army_supply >= 40 || army_supply >= 60) {
							if (count_units_plus_production(st, unit_types::ultralisk_cavern) == 0) {
								army = [army](state&st) {
									return nodelay(st, unit_types::ultralisk_cavern, army);
								};
							}
						}
					}
				}

				if (army_supply > enemy_army_supply && drone_count < 100 && army_supply >= 30) {
					if (count_production(st, unit_types::drone) < 8) {
						army = [army](state&st) {
							return nodelay(st, unit_types::drone, army);
						};
					}
				}

				if (force_expand && count_production(st, unit_types::hatchery) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}

				return army(st);
			};

			execute_build(false, build);

			place_static_defence();
			place_expos();

			multitasking::sleep(15 * 5);
		}

	}

};

