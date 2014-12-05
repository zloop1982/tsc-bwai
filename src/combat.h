
namespace combat {
;

struct defence_spot {
	xy pos;
	a_map<unit_type*, int> unit_counts;
	a_vector<std::tuple<grid::build_square*, double>> squares;
	double value;
};
a_vector<defence_spot> defence_spots;

tsc::dynamic_bitset my_base;
tsc::dynamic_bitset op_base;

void update_base(tsc::dynamic_bitset&base, const a_vector<unit*>&buildings) {
	base.reset();

	tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
	a_deque<std::tuple<grid::build_square*, xy>> open;
	for (unit*u : buildings) {
		for (auto*v : u->building->build_squares_occupied) {
			xy pos = v->pos;
			open.emplace_back(&grid::get_build_square(pos), pos);
			size_t index = grid::build_square_index(pos);
			visited.set(index);
			base.set(index);
		}
	}
	while (!open.empty()) {
		grid::build_square*bs;
		xy origin;
		std::tie(bs, origin) = open.front();
		open.pop_front();

		auto add = [&](int n) {
			grid::build_square*nbs = bs->get_neighbor(n);
			if (!nbs) return;
			if (!nbs->entirely_walkable) return;
			if (diag_distance(nbs->pos - origin) >= 32 * 12) return;
			size_t index = grid::build_square_index(*nbs);
			if (visited.test(index)) return;
			visited.set(index);
			base.set(index);

			open.emplace_back(nbs, origin);
		};
		add(0);
		add(1);
		add(2);
		add(3);
	}
}

void update_my_base() {
	update_base(my_base, my_buildings);
}
void update_op_base() {
	update_base(op_base, enemy_buildings);
}

void generate_defence_spots() {

	a_multimap<grid::build_square*, std::tuple<unit_type*, double>> hits;

	auto spread_from = [&](xy pos, unit_type*ut) {
		tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
		a_deque<std::tuple<grid::build_square*, double>> open;
		open.emplace_back(&grid::get_build_square(pos), 0.0);
		visited.set(grid::build_square_index(pos));
		while (!open.empty()) {
			grid::build_square*bs;
			double distance;
			std::tie(bs, distance) = open.front();
			open.pop_front();

			auto add = [&](int n) {
				grid::build_square*nbs = bs->get_neighbor(n);
				if (!nbs) return;
				if (!nbs->entirely_walkable) return;
				size_t index = grid::build_square_index(*nbs);
				if (visited.test(index)) return;
				visited.set(index);
				double d = distance + 1;
				if (my_base.test(index)) {
					hits.emplace(nbs, std::make_tuple(ut, d));
					return;
				}
				open.emplace_back(nbs, d);
			};
			add(0);
			add(1);
			add(2);
			add(3);
		}
	};

	for (unit*u : enemy_units) {
		if (u->building) continue;
		if (u->gone) continue;
		spread_from(u->pos, u->type);
	}
	if (hits.empty()) {
		// TODO: change this to spread from starting locations (not in my_base) instead
		spread_from(xy(grid::map_width / 2, grid::map_height / 2), unit_types::zergling);
	}

	a_vector<defence_spot> new_defence_spots;
	for (auto&v : hits) {
		grid::build_square*bs = v.first;
		unit_type*ut = std::get<0>(v.second);
		double value = std::get<1>(v.second);
		defence_spot*ds = nullptr;
		for (auto&v : new_defence_spots) {
			for (auto&v2 : v.squares) {
				if (diag_distance(bs->pos - std::get<0>(v2)->pos) < 32*2) {
					ds = &v;
					break;
				}
			}
			if (ds) break;
		}
		if (!ds) {
			new_defence_spots.emplace_back();
			ds = &new_defence_spots.back();
		}
		ds->unit_counts[ut] += 1;
		bool found = false;
		for (auto&v : ds->squares) {
			if (std::get<0>(v) == bs) {
				std::get<1>(v) += value;
				found = true;
				break;
			}
		}
		if (!found) ds->squares.emplace_back(bs, value);
	}
	for (auto&v : new_defence_spots) {
		std::sort(v.squares.begin(), v.squares.end(), [&](const std::tuple<grid::build_square*, double>&a, const std::tuple<grid::build_square*, double>&b) {
			return std::get<1>(a) < std::get<1>(b);
		});
		v.pos = std::get<0>(v.squares.front())->pos + xy(16, 16);
		v.value = std::get<1>(v.squares.front());
	}
	std::sort(new_defence_spots.begin(), new_defence_spots.end(), [&](const defence_spot&a, const defence_spot&b) {
		return a.value < b.value;
	});
	defence_spots = std::move(new_defence_spots);

	log("there are %d defence spots\n", defence_spots.size());

}

void generate_defence_spots_task() {

	while (true) {

		generate_defence_spots();

		tsc::high_resolution_timer ht;

// 		combat_eval::eval test;
// 		for (int i = 0; i < 50; ++i) test.add_unit(units::get_unit_stats(unit_types::vulture, units::my_player), 0);
// 		for (int i = 0; i < 20; ++i) test.add_unit(units::get_unit_stats(unit_types::marine, units::my_player), 1);
// 		for (int i = 0; i < 15; ++i) test.add_unit(units::get_unit_stats(unit_types::siege_tank_tank_mode, units::my_player), 1);
// 		test.add_unit(units::get_unit_stats(unit_types::missile_turret, units::my_player), 1);
// // 		test.add_unit(units::get_unit_stats(unit_types::vulture, units::my_player), 0);
// // 		test.add_unit(units::get_unit_stats(unit_types::siege_tank_tank_mode, units::my_player), 1);
// 
// 		test.run();
// 
// 		log("took %f\n", ht.elapsed());
// 		
// 		log("start supply: %g %g\n", test.teams[0].start_supply, test.teams[1].start_supply);
// 		log("end supply: %g %g\n", test.teams[0].end_supply, test.teams[1].end_supply);
// 
// 		log("damage dealt: %g %g\n", test.teams[0].damage_dealt, test.teams[1].damage_dealt);
// 
// 		xcept("stop");

		multitasking::sleep(90);
	}

}

template<typename pred_T>
void find_nearby_entirely_walkable_tiles(xy pos, pred_T&&pred) {
	tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
	a_deque<std::tuple<grid::build_square*, xy>> open;
	open.emplace_back(&grid::get_build_square(pos), pos);
	size_t index = grid::build_square_index(pos);
	visited.set(index);
	while (!open.empty()) {
		grid::build_square*bs;
		xy origin;
		std::tie(bs, origin) = open.front();
		open.pop_front();
		if (!pred(bs->pos)) continue;

		auto add = [&](int n) {
			grid::build_square*nbs = bs->get_neighbor(n);
			if (!nbs) return;
			if (!nbs->entirely_walkable) return;
			if (diag_distance(nbs->pos - origin) >= 32 * 12) return;
			size_t index = grid::build_square_index(*nbs);
			if (visited.test(index)) return;
			visited.set(index);

			open.emplace_back(nbs, origin);
		};
		add(0);
		add(1);
		add(2);
		add(3);
	}
}

struct combat_unit {
	unit*u = nullptr;
	enum { action_idle, action_offence, action_tactic };
	int action = action_idle;
	enum { subaction_idle, subaction_move, subaction_fight, subaction_move_directly, subaction_use_ability, subaction_repair };
	int subaction = subaction_idle;
	xy defend_spot;
	xy goal_pos;
	int last_fight = 0;
	int last_run = 0;
	int last_processed_fight = 0;
	unit*target = nullptr;
	xy target_pos;
	int last_wanted_a_lift = 0;
	upgrade_type*ability = nullptr;
	int last_used_special = 0;
	int last_nuke = 0;
	int unsiege_counter = 0;
};
a_unordered_map<unit*, combat_unit> combat_unit_map;

a_vector<combat_unit*> live_combat_units;
a_vector<combat_unit*> idle_combat_units;

void update_combat_units() {
	live_combat_units.clear();
	size_t worker_count = 0;
	for (unit*u : my_units) {
		if (u->building) continue;
		if (!u->is_completed) continue;
// 		if (u->type->is_worker && !u->force_combat_unit) {
// 			if (u->controller->action == unit_controller::action_build) continue;
// 			if (u->controller->action == unit_controller::action_scout) continue;
// 		}
		if (u->controller->action == unit_controller::action_build) continue;
		if (u->controller->action == unit_controller::action_scout) continue;
		if (u->type->is_non_usable) continue;
		//if (worker_count > 1 && u->type->is_worker && !u->force_combat_unit) continue;
		if (u->type->is_worker) ++worker_count;
		combat_unit&c = combat_unit_map[u];
		if (!c.u) c.u = u;
		live_combat_units.push_back(&c);
	}
	idle_combat_units.clear();
	for (auto*cu : live_combat_units) {
		if (cu->action == combat_unit::action_idle) idle_combat_units.push_back(cu);
	}
}

void defence() {

	if (defence_spots.empty()) return;

	auto send_to = [&](combat_unit*cu, defence_spot*s) {
		if (cu->defend_spot == s->pos) return;

		cu->defend_spot = s->pos;
		cu->goal_pos = s->pos;

		cu->subaction = combat_unit::subaction_move;
		cu->target_pos = s->pos;
	};

	for (auto*cu : idle_combat_units) {
		if (cu->u->type->is_worker && !cu->u->force_combat_unit) continue;
		if (current_frame - cu->last_fight <= 40) continue;
		auto&s = defence_spots.front();
		send_to(cu, &s);
	}

}

a_unordered_map<combat_unit*, int> pickup_taken;

void give_lifts(combat_unit*dropship, a_vector<combat_unit*>&allies, int process_uid) {

	a_vector<combat_unit*> wants_a_lift;
	for (auto*cu : allies) {
		double path_time = unit_pathing_distance(cu->u, cu->goal_pos) / cu->u->stats->max_speed;
		double pickup_time = diag_distance(dropship->u->pos - cu->u->pos) / dropship->u->stats->max_speed;
		double air_time = diag_distance(cu->goal_pos - cu->u->pos) / dropship->u->stats->max_speed;
		if (cu->u->is_loaded) {
			if (current_frame - cu->last_wanted_a_lift <= 15 * 5) {
				wants_a_lift.push_back(cu);
				continue;
			}
			if (path_time + 15 * 5 < pickup_time + air_time) continue;
		} else if (path_time <= (pickup_time + air_time) + 15 * 10) continue;
		log("%s : path_time %g pickup_time %g air_time %g\n", cu->u->type->name, path_time, pickup_time, air_time);
		wants_a_lift.push_back(cu);
		cu->last_wanted_a_lift = current_frame;
	}
	int space = dropship->u->type->space_provided;
	for (unit*lu : dropship->u->loaded_units) {
		space -= lu->type->space_required;
	}
	a_unordered_set<combat_unit*> units_to_pick_up;
	while (space > 0) {
		combat_unit*nu = get_best_score(wants_a_lift, [&](combat_unit*nu) {
			if (nu->u->type->is_flyer) return std::numeric_limits<double>::infinity();
			if (nu->u->is_loaded || units_to_pick_up.find(nu) != units_to_pick_up.end()) return std::numeric_limits<double>::infinity();
			if (nu->u->type->space_required > space) return std::numeric_limits<double>::infinity();
			if (pickup_taken[nu] == process_uid) return std::numeric_limits<double>::infinity();
			return diag_distance(nu->u->pos - dropship->u->pos);
		}, std::numeric_limits<double>::infinity());
		if (!nu) break;
		units_to_pick_up.insert(nu);
		space -= nu->u->type->space_required;
	}

	for (unit*u : dropship->u->loaded_units) {
		bool found = false;
		for (auto*cu : wants_a_lift) {
			if (cu->u == u) {
				found = true;
				break;
			}
		}
		if (!found) {
			if (current_frame >= dropship->u->controller->noorder_until) {
				dropship->u->game_unit->unload(u->game_unit);
				dropship->u->controller->noorder_until = current_frame + 4;
			}
		}
	}

	for (auto*cu : units_to_pick_up) {
		pickup_taken[cu] = process_uid;
		// todo: find the best spot to meet the dropship
		cu->subaction = combat_unit::subaction_move;
		cu->target_pos = dropship->u->pos;
	}

	combat_unit*pickup = get_best_score(units_to_pick_up, [&](combat_unit*cu) {
		return diag_distance(cu->u->pos - dropship->u->pos);
	});
	if (pickup) {
		dropship->subaction = combat_unit::subaction_move;
		dropship->target_pos = pickup->u->pos;
		if (diag_distance(pickup->u->pos - dropship->u->pos) <= 32 * 4) {
			dropship->subaction = combat_unit::subaction_idle;
			dropship->u->controller->action = unit_controller::action_idle;
			if (current_frame >= dropship->u->controller->noorder_until) {
				pickup->u->game_unit->rightClick(dropship->u->game_unit);
				pickup->u->controller->noorder_until = current_frame + 15;
				dropship->u->controller->noorder_until = current_frame + 4;
			}
		}
	}
}

struct group_t {
	double value;
	a_vector<unit*> enemies;
	a_vector<combat_unit*> allies;
	tsc::dynamic_bitset threat_area;
};
a_vector<group_t> groups;
multitasking::mutex groups_mut;
tsc::dynamic_bitset entire_threat_area;
tsc::dynamic_bitset entire_threat_area_edge;

void update_group_area(group_t&g) {
	g.threat_area.reset();
	for (unit*e : g.enemies) {
		tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
		a_deque<xy> walk_open;
		a_deque<std::tuple<xy, xy>> threat_open;
		double threat_radius = 32;
		if (e->stats->ground_weapon && e->stats->ground_weapon->max_range > threat_radius) threat_radius = e->stats->ground_weapon->max_range;
		if (e->stats->air_weapon && e->stats->air_weapon->max_range > threat_radius) threat_radius = e->stats->air_weapon->max_range;
		if (e->type == unit_types::bunker) threat_radius = 32 * 6;
		threat_radius += e->type->width;
		threat_radius += 48;
		if (e->type == unit_types::siege_tank_siege_mode) threat_radius += 128;
		//double walk_radius = e->stats->max_speed * 15 * 2;
		double walk_radius = e->stats->max_speed * 20;
		//double walk_radius = 32;
		walk_open.emplace_back(e->pos);
		visited.set(grid::build_square_index(e->pos));
		while (!walk_open.empty() || !threat_open.empty()) {
			xy pos;
			xy origin;
			bool is_walk = !walk_open.empty();
			if (is_walk) {
				pos = walk_open.front();
				walk_open.pop_front();
			} else {
				std::tie(pos, origin) = threat_open.front();
				threat_open.pop_front();
			}
			g.threat_area.set(grid::build_square_index(pos));
			for (int i = 0; i < 4; ++i) {
				xy npos = pos;
				if (i == 0) npos.x += 32;
				if (i == 1) npos.y += 32;
				if (i == 2) npos.x -= 32;
				if (i == 3) npos.y -= 32;
				if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) continue;
				size_t index = grid::build_square_index(npos);
				if (visited.test(index)) continue;
				visited.set(index);
				double walk_d = (npos - e->pos).length();
				if (is_walk) {
					bool out_of_range = !e->type->is_flyer && !grid::get_build_square(npos).entirely_walkable;
					if (!out_of_range) out_of_range = walk_d >= walk_radius;
					if (out_of_range) threat_open.emplace_back(npos, npos);
					else walk_open.emplace_back(npos);
				} else {
					double d = (npos - origin).length();
					if (d < threat_radius) {
						threat_open.emplace_back(npos, origin);
					}
				}
			}
		}
	}
}

a_deque<xy> find_bigstep_path(unit_type*ut, xy from, xy to) {
	if (ut->is_flyer) {
		return flyer_pathing::find_bigstep_path(from, to);
	} else {
		auto path = square_pathing::find_path(square_pathing::get_pathing_map(ut), from, to);
		a_deque<xy> r;
		for (auto*n : path) r.push_back(n->pos);
		return r;
	}
};

void update_groups() {
	std::lock_guard<multitasking::mutex> l(groups_mut);

	for (auto&g : groups) {
		xy pos;
		int visible_count = 0;
		for (unit*e : enemy_units) {
			if (e->visible) {
				pos += e->pos;
				++visible_count;
			}
		}
		if (visible_count) {
			pos /= visible_count;
// 			for (unit*e : enemy_units) {
// 				if (!e->visible) {
// 					e->pos = pos;
// 				}
// 			}
		}
	}

	groups.clear();

	a_vector<std::tuple<double, unit*>> sorted_enemy_units;
	for (unit*e : enemy_units) {
		double d = get_best_score_value(my_units, [&](unit*u) {
			if (u->type->is_non_usable) return std::numeric_limits<double>::infinity();
			return diag_distance(e->pos - u->pos);
		}, std::numeric_limits<double>::infinity());
		sorted_enemy_units.emplace_back(d, e);
	}
	std::sort(sorted_enemy_units.begin(), sorted_enemy_units.end());

	for (auto&v : sorted_enemy_units) {
		unit*e = std::get<1>(v);
		//if (e->type->is_worker) continue;
		if (!buildpred::attack_now) {
			//if (!e->stats->ground_weapon && !e->stats->air_weapon && !e->type->is_resource_depot) continue;
		}
		if (e->gone) continue;
		if (e->invincible) continue;
		if (e->type->is_non_usable) continue;
		//if (!buildpred::attack_now && op_base.test(grid::build_square_index(e->pos))) continue;
		group_t*g = nullptr;
		for (auto&v : groups) {
// 			for (unit*ne : v.enemies) {
// 				double d = units_distance(e, ne);
// 				bool is_near = false;
// 				if (d <= e->stats->sight_range) is_near = true;
// 				if (d <= ne->stats->sight_range) is_near = true;
// 				if (d <= 32 * 6) is_near = true;
// 				if (!is_near) continue;
// 				if (!square_pathing::unit_can_reach(e, e->pos, ne->pos)) continue;
// 				g = &v;
// 				break;
// 			}
//			if (g) break;
			unit*ne = v.enemies.front();
			double d = units_distance(e, ne);
			if (d >= 32 * 15) continue;
			if (!square_pathing::unit_can_reach(e, e->pos, ne->pos)) continue;
			g = &v;
			break;
		}
		if (!g) {
			groups.emplace_back();
			g = &groups.back();
			g->threat_area.resize(grid::build_grid_width*grid::build_grid_height);
		}
		g->enemies.push_back(e);
	}
	for (auto&g : groups) {
		int buildings = 0;
		for (unit*e : g.enemies) {
			if (e->building && !e->stats->air_weapon && !e->stats->ground_weapon && e->type != unit_types::bunker) ++buildings;
		}
		if (buildings != g.enemies.size()) {
			for (auto i = g.enemies.begin(); i != g.enemies.end();) {
				unit*e = *i;
				if (e->building && !e->stats->air_weapon && !e->stats->ground_weapon && e->type != unit_types::bunker) i = g.enemies.erase(i);
				else ++i;
			}
		}
	}
	entire_threat_area.reset();
	entire_threat_area_edge.reset();
	for (auto&g : groups) {
		update_group_area(g);
	}
// 	for (auto i = groups.begin(); i != groups.end();) {
// 		auto&g = *i;
// 		bool merged = false;
// 		for (auto&g2 : groups) {
// 			if (&g2 == &g) break;
// 			if ((g.threat_area&g2.threat_area).none()) continue;
// 			for (unit*u : g.enemies) {
// 				g2.enemies.push_back(u);
// 			}
// 			update_group_area(g2);
// 			merged = true;
// 			break;
// 		}
// 		if (merged) i = groups.erase(i);
// 		else ++i;
// 	}
	for (auto&g : groups) {
		entire_threat_area |= g.threat_area;
	}
	for (size_t idx : entire_threat_area) {
		size_t xidx = idx % (size_t)grid::build_grid_width;
		size_t yidx = idx / (size_t)grid::build_grid_width;
		auto test = [&](size_t index) {
			if (!entire_threat_area.test(index)) entire_threat_area_edge.set(index);
		};
		if (xidx != grid::build_grid_last_x) test(idx + 1);
		if (yidx != grid::build_grid_last_y) test(idx + grid::build_grid_width);
		if (xidx != 0) test(idx - 1);
		if (yidx != 0) test(idx - grid::build_grid_width);
	}
	for (auto&g : groups) {
		double value = 0.0;
		for (unit*e : g.enemies) {
			//if (e->type->is_worker) continue;
			bool is_attacking_my_workers = e->order_target && e->order_target->owner == players::my_player && e->order_target->type->is_worker;
			if (is_attacking_my_workers) {
				if (!my_base.test(grid::build_square_index(e->order_target->pos))) is_attacking_my_workers = false;
			}
			bool is_attacking_my_base = e->order_target && my_base.test(grid::build_square_index(e->order_target->pos));
			if (is_attacking_my_base || is_attacking_my_workers) e->high_priority_until = current_frame + 15 * 15;
			if (e->high_priority_until >= current_frame) {
				value += e->minerals_value;
				value += e->gas_value * 2;
				value += 1000;
			} else {
				if (e->type->is_worker) value += 200;
				value -= e->minerals_value;
				value -= e->gas_value * 2;
			}
			//if (e->type->is_building) value /= 4;
			//if (my_base.test(grid::build_square_index(e->pos))) value *= 4;
			//if (my_base.test(grid::build_square_index(e->pos))) value *= 4;
			//if (op_base.test(grid::build_square_index(e->pos))) value /= 4;
			if (my_base.test(grid::build_square_index(e->pos))) value += 1000;
		}
// 		unit*nb = get_best_score(my_buildings, [&](unit*u) {
// 			return unit_pathing_distance(unit_types::scv, u->pos, g.enemies.front()->pos);
// 		});
// 		if (nb) {
// 			for (auto*n : square_pathing::find_path(square_pathing::get_pathing_map(unit_types::scv), nb->pos, g.enemies.front()->pos)) {
// 				for (unit*e : enemy_units) {
// 					if (e->gone) continue;
// 					if (e->type->is_non_usable) continue;
// 					weapon_stats*w = e->stats->ground_weapon;
// 					if (!w) continue;
// 					if (diag_distance(e->pos - n->pos) <= w->max_range) {
// 						value -= e->minerals_value;
// 						value -= e->gas_value;
// 					}
// 				}
// 			}
// 		}
		g.value = value;
	}
	std::sort(groups.begin(), groups.end(), [&](const group_t&a, const group_t&b) {
		return a.value > b.value;
	});

	for (auto&g : groups) {
		if (true) {
			a_map<unit_type*, int> counts;
			for (unit*e : g.enemies) ++counts[e->type];
			log("group value %g - enemies -", g.value);
			for (auto&v : counts) {
				log("%dx%s ", std::get<1>(v), std::get<0>(v)->name);
			}
			log("\n");
		}
	}

	a_unordered_set<combat_unit*> available_units;
	for (auto*c : live_combat_units) {
		if (c->action == combat_unit::action_tactic) continue;
		available_units.insert(c);
	}

	a_unordered_map<combat_unit*, a_unordered_set<group_t*>> can_reach_group;
	for (auto*c : available_units) {
		//if (c->u->is_flying || c->u->is_loaded) continue;
		for (auto&g : groups) {
			bool okay = true;
			size_t count = 0;
			//for (auto*n : square_pathing::find_path(square_pathing::get_pathing_map(c->u->type), c->u->pos, g.enemies.front()->pos)) {
			//	xy pos = n->pos;
			for (xy pos : find_bigstep_path(c->u->type, c->u->pos, g.enemies.front()->pos)) {
				size_t index = grid::build_square_index(pos);
				if (entire_threat_area.test(index)) {
					//log("threat area found after %d\n", count);
					//okay = g.threat_area.test(index);
					//log("%p -> %p - found after %d, okay %d\n", c, &g, count, okay);
					//break;
					bool found = false;
					for (auto&g2 : groups) {
						if (&g2 == &g) continue;
						for (unit*e : g2.enemies) {
							double wr = 0.0;
							if (e->type == unit_types::bunker) {
								wr = 32 * 5;
							} else {
								weapon_stats*w = c->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
								if (!w) continue;
								wr = w->max_range;
							}
							double d = diag_distance(pos - e->pos);
							//log("%s - %d - d to %s is %g\n", c->u->type->name, count, e->type->name, d);
							if (d <= wr || d <= 32 * 10) {
								found = true;
								okay = &g2 == &g;
								break;
							}
						}
						if (found) break;
					}
					if (found) {
						//log("%p -> %p - found after %d, okay %d\n", c, &g, count, okay);
					}
					if (found) break;
				}
				++count;
			}
			if (okay) can_reach_group[c].insert(&g);
			multitasking::yield_point();
		}
	}

// 	for (auto i = available_units.begin(); i != available_units.end();) {
// 		auto*cu = *i;
// // 		if (cu->u->type->is_worker) {
// // 			++i;
// // 			continue;
// // 		}
// 		size_t index = grid::build_square_index(cu->u->pos);
// 		group_t*inside_group = nullptr;
// 		for (auto&g : groups) {
// 			bool can_attack_any = false;
// 			for (unit*e : g.enemies) {
// 				weapon_stats*w = e->is_flying ? cu->u->stats->air_weapon : cu->u->stats->ground_weapon;
// 				if (w) {
// 					can_attack_any = true;
// 					break;
// 				}
// 			}
// 			if (!can_attack_any) continue;
// 			if (g.threat_area.test(index)) {
// 				if (!inside_group || diag_distance(cu->u->pos - g.enemies.front()->pos) < diag_distance(cu->u->pos - inside_group->enemies.front()->pos)) {
// 					inside_group = &g;
// 				}
// 			}
// 		}
// 		if (inside_group) {
// 			//log("%s is inside group %p\n", cu->u->type->name, inside_group);
// 			if (cu->u->type->is_worker) {
// 				if (inside_group->enemies.size() == 1 && inside_group->enemies.front()->type->is_worker) {
// 					++i;
// 					continue;
// 				}
// 			}
// 			inside_group->allies.push_back(cu);
// 			i = available_units.erase(i);
// 		} else ++i;
// 	}
// 	for (auto&g : groups) {
// 		combat_eval::eval eval;
// 		auto addu = [&](unit*u, int team) {
// 			auto&c = eval.add_unit(u, team);
// 		};
// 		for (auto*a : g.allies) addu(a->u, 0);
// 		for (unit*e : g.enemies) addu(e, 1);
// 		eval.run();
// 		bool won = eval.teams[0].end_supply > eval.teams[1].end_supply;
// 		if (!won) {
// 			for (auto i = g.allies.begin(); i != g.allies.end();) {
// 				auto*c = *i;
// 				if (c->u->type->is_worker) {
// 					i = g.allies.erase(i);
// 					available_units.insert(c);
// 				} else ++i;
// 			}
// 		}
// 	}

// 	size_t avail_workers = 0;
// 	for (auto i = available_units.begin(); i != available_units.end();) {
// 		auto*c = *i;
// 		if (c->u->type->is_worker) {
// 			if (++avail_workers >= (my_workers.size() + 9) / 10) {
// 				if (c->action != combat_unit::action_idle || c->subaction != combat_unit::subaction_idle) {
// 					c->action = combat_unit::action_idle;
// 					c->subaction = combat_unit::subaction_idle;
// 					c->u->controller->action = unit_controller::action_idle;
// 				}
// 				i = available_units.erase(i);
// 				continue;
// 			}
// 		}
// 		++i;
// 	}

	size_t group_idx = 0;
	for (auto&g : groups) {
		bool is_attacking = false;
		bool is_base_defence = false;
		bool is_just_one_worker = g.enemies.size() == 1 && g.enemies.front()->type->is_worker;
		for (unit*e : g.enemies) {
			if (current_frame - e->last_attacking <= 30) is_attacking = true;
			if (!is_base_defence) is_base_defence = my_base.test(grid::build_square_index(g.enemies.front()->pos));
		}
		while (true) {
			std::tuple<bool, bool, double> best_score;
			combat_unit*best_unit = nullptr;
			auto get_score_for = [&](combat_unit*c) {
				combat_eval::eval eval;
				auto addu = [&](unit*u, int team) {
					auto&c = eval.add_unit(u, team);
				};
				for (auto*a : g.allies) addu(a->u, 0);
				for (unit*e : g.enemies) addu(e, 1);
				addu(c->u, 0);
				eval.run();
				bool done = eval.teams[1].end_supply == 0 && eval.teams[0].end_supply >= eval.teams[0].start_supply / 2 + 4;
				double val = eval.teams[0].score - eval.teams[1].score;
				if (g.threat_area.test(grid::build_square_index(c->u->pos))) val += 1000;
				bool is_combat_unit = !(c->u->type->is_worker && !c->u->force_combat_unit);
				// 				if (!is_combat_unit) {
				// 					done = eval.teams[0].start_supply >= eval.teams[0].start_supply;
				// 				}
				auto score = std::make_tuple(is_combat_unit, done, val);
				return score;
			};
// 			for (auto*c : available_units) {
// 				if (!c->u->stats->ground_weapon && !c->u->stats->air_weapon) continue;
// 				if (!c->u->is_loaded && !square_pathing::unit_can_reach(c->u, c->u->pos, g.enemies.front()->pos)) continue;
// 				size_t index = grid::build_square_index(c->u->pos);
// 				if (g.threat_area.test(index) && !entire_threat_area.test(index)) continue;
// 				auto score = get_score_for(c);
// 				if (!best_unit || score > best_score) {
// 					best_score = score;
// 					best_unit = c;
// 				}
// 			}
			a_unordered_set<combat_unit*> blacklist;
			auto get_nearest_unit = [&]() {
				return get_best_score(available_units, [&](combat_unit*c) {
					if (!c->u->stats->ground_weapon && !c->u->stats->air_weapon) return std::numeric_limits<double>::infinity();
					if (is_just_one_worker && !is_attacking && c->u->type->is_worker) return std::numeric_limits<double>::infinity();
					if (c->u->type->is_worker && !c->u->force_combat_unit && !is_base_defence) return std::numeric_limits<double>::infinity();
					//if (c->u->type->is_worker) return std::numeric_limits<double>::infinity();
					if (!c->u->is_loaded && !square_pathing::unit_can_reach(c->u, c->u->pos, g.enemies.front()->pos)) return std::numeric_limits<double>::infinity();
					if (blacklist.count(c)) return std::numeric_limits<double>::infinity();
					if (!can_reach_group[c].count(&g)) return std::numeric_limits<double>::infinity();;
					double d;
					if (c->u->is_loaded) d = diag_distance(g.enemies.front()->pos - c->u->pos);
					else d = units_pathing_distance(c->u, g.enemies.front());
					return d;
				}, std::numeric_limits<double>::infinity());
			};
			auto is_useful = [&](combat_unit*c) {
				//if (!can_reach_group[c].count(&g)) return false;
				if (!c->u->stats->ground_weapon && !c->u->stats->air_weapon) return true;
				for (unit*e : g.enemies) {
					weapon_stats*w = e->is_flying ? c->u->stats->air_weapon : c->u->stats->ground_weapon;
					if (w) return true;
				}
				return false;
			};
			combat_unit*nearest_unit = get_nearest_unit();
			while (nearest_unit && !is_useful(nearest_unit)) {
				//log("%p -> %p is not useful\n", nearest_unit, &g);
				if (blacklist.count(nearest_unit)) {
					nearest_unit = nullptr;
					break;
				}
				blacklist.insert(nearest_unit);
				nearest_unit = get_nearest_unit();
			}
			if (nearest_unit) {
				best_unit = nearest_unit;
				best_score = get_score_for(best_unit);
			}
			if (!best_unit) log("failed to find unit for group %d\n", group_idx);
// 			if (!best_unit) {
// 				for (auto*c : g.allies) {
// 					available_units.insert(c);
// 				}
// 				g.allies.clear();
// 			}
			if (!best_unit) break;
// 			if (best_unit->u->type->is_worker && !best_unit->u->force_combat_unit && !is_base_defence) break;
// 			if (best_unit->u->type->is_worker && !best_unit->u->force_combat_unit && !is_attacking) break;
// 			//if (best_unit->u->type->is_worker && !best_unit->u->force_combat_unit && std::get<2>(best_score) >= 0) break;
			//log("add %s to group %d with score %d %g\n", best_unit->u->type->name, group_idx, std::get<0>(best_score), std::get<1>(best_score));
			g.allies.push_back(best_unit);
			available_units.erase(best_unit);
			if (std::get<1>(best_score)) break;
			//if (!std::get<0>(best_score)) break;
			if (is_just_one_worker) break;
		}
		log("group %d: %d allies %d enemies\n", group_idx, g.allies.size(), g.enemies.size());
		++group_idx;
	}

	for (auto*c : available_units) {
		if (c->u->type->is_worker && !c->u->force_combat_unit) {
			if (c->action!=combat_unit::action_idle || c->subaction != combat_unit::subaction_idle) {
				c->action = combat_unit::action_idle;
				c->subaction = combat_unit::subaction_idle;
				c->u->controller->action = unit_controller::action_idle;
			}
		} else {
			if (c->action == combat_unit::action_offence) {
				c->action = combat_unit::action_idle;
				c->subaction = combat_unit::subaction_idle;
				c->u->controller->action = unit_controller::action_idle;
			}
		}
	}

	if (!groups.empty()) {
		auto*largest_group = get_best_score_p(groups, [&](const group_t*g) {
			return -(int)g->allies.size();
		});
		if (largest_group && !largest_group->allies.empty()) {
			size_t worker_count = 0;
			for (auto*c : available_units) {
				if (c->u->type->is_worker && !c->u->force_combat_unit) {
					if (++worker_count > my_workers.size() / 10) continue;
				}
				//groups.front().allies.push_back(c);
				largest_group->allies.push_back(c);
			}
		}
	} else {
		for (auto*c : available_units) {
			if (c->u->type->is_worker && !c->u->force_combat_unit) continue;
			c->action = combat_unit::action_idle;
		}
	}
	available_units.clear();

	for (auto&g : groups) {
		xy pos = g.enemies.front()->pos;
		a_vector<combat_unit*> dropships;
		a_vector<combat_unit*> moving_units;
		for (auto*cu : g.allies) {
			cu->goal_pos = pos;
			cu->action = combat_unit::action_offence;
			if (current_frame - cu->last_fight <= 40) continue;
			cu->subaction = combat_unit::subaction_move;
			cu->target_pos = pos;
			if (cu->u->type == unit_types::dropship) dropships.push_back(cu);
			moving_units.push_back(cu);
		}
		int process_uid = current_frame;
		for (auto*cu : dropships) {
			give_lifts(cu, moving_units, process_uid);
		}
	}
	
}

a_unordered_map<unit*, int> dropship_spread;
a_unordered_map<unit*, double> focus_fire;
a_unordered_map<unit*, double> prepared_damage;
a_unordered_map<unit*, bool> is_being_healed;
a_vector<xy> spread_positions;

//a_vector<combat_unit*> spider_mine_layers;
a_unordered_set<unit*> active_spider_mine_targets;
a_unordered_set<unit*> my_spider_mines_in_range_of;

//a_unordered_map<unit*, double> nuke_damage;
a_unordered_set<unit*> lockdown_target_taken;

void prepare_attack() {
	focus_fire.clear();
	prepared_damage.clear();
	dropship_spread.clear();
	is_being_healed.clear();
	spread_positions.clear();
	//spider_mine_layers.clear();

	active_spider_mine_targets.clear();
	my_spider_mines_in_range_of.clear();
	for (unit*u : my_units_of_type[unit_types::spider_mine]) {
		if (u->order_target) active_spider_mine_targets.insert(u->order_target);
		for (unit*e : enemy_units) {
			if (e->is_flying || e->type->is_hovering) continue;
			if (units_distance(u, e) <= 32 * 3) my_spider_mines_in_range_of.insert(e);
		}
	}
	for (unit*u : enemy_units) {
		if (!u->visible) continue;
		if (u->type != unit_types::spider_mine) continue;
		if (u->order_target) active_spider_mine_targets.insert(u->order_target);
	}

	//nuke_damage.clear();
	lockdown_target_taken.clear();
}

a_vector<std::tuple<xy, double>> nuke_test;

void finish_attack() {
	a_vector<combat_unit*> spider_mine_layers;
	a_vector<combat_unit*> close_enough;

	for (combat_unit*c : live_combat_units) {
		if (players::my_player->has_upgrade(upgrade_types::spider_mines) && c->u->spider_mine_count) {
			unit*target = nullptr;
			unit*nearby_target = nullptr;
			for (unit*e : enemy_units) {
				if (!e->visible) continue;
				if (e->type->is_building) continue;
				if (e->is_flying) continue;
				if (e->type->is_hovering) continue;
				if (e->type->is_non_usable) continue;
				if (e->invincible) continue;
				double d = diag_distance(e->pos - c->u->pos);
				if (d <= 32 * 5) {
					nearby_target = e;
				}
				if (d <= 32 * 3) {
					target = e;
					break;
				}
			}
			if (nearby_target) spider_mine_layers.push_back(c);
			if (target) close_enough.push_back(c);
		}
	}

	auto lay_mine = [&](combat_unit*c) {
		if (current_frame - c->last_used_special >= 15) {
			c->u->game_unit->useTech(upgrade_types::spider_mines->game_tech_type, BWAPI::Position(c->u->pos.x, c->u->pos.y));
			c->last_used_special = current_frame;
			c->u->controller->noorder_until = current_frame + 15;
		}
	};
	bool lay = false;
	if (close_enough.size() >= 4 || close_enough.size() >= spider_mine_layers.size()) {
		for (auto*c : close_enough) {
			lay = true;
			break;
		}
	} else {
		for (auto*c : close_enough) {
			bool is_solo = true;
			for (auto*c2 : spider_mine_layers) {
				if (units_distance(c->u, c2->u) <= 32) {
					is_solo = false;
					break;
				}
			}
			if (is_solo || c->u->hp <= 60) {
				lay = true;
				break;
			}
		}
	}
	if (lay) {
		for (auto*c : close_enough) {
			lay_mine(c);
		}
	}

	a_vector<combat_unit*> ghosts;
	for (auto*c : live_combat_units) {
		if (c->u->type == unit_types::ghost) ghosts.push_back(c);
	}
	if (players::my_player->has_upgrade(upgrade_types::personal_cloaking)) {
		// todo: some better logic here. cloak if being attacked and there are no detectors in range
		//       uncloak when safe?
		for (auto*c : ghosts) {
			if (c->u->hp < 40) {
				if (!c->u->cloaked && c->u->energy>140) {
					c->subaction = combat_unit::subaction_use_ability;
					c->ability = upgrade_types::personal_cloaking;
				}
			}
		}
	}
	auto test_in_range = [&](combat_unit*c, unit*e, xy stand_pos, bool&in_attack_range, bool&is_revealed) {
		if (!in_attack_range) {
			weapon_stats*w = c->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (w) {
				double d = diag_distance(e->pos - stand_pos);
				if (d <= w->max_range + 32) {
					in_attack_range = true;
				}
			}
		}
		if (!is_revealed) {
			if (e->type->is_detector) {
				double d = diag_distance(e->pos - stand_pos);
				if (d <= e->stats->sight_range + 32) is_revealed = true;
			}
		}
	};
	if (players::my_player->has_upgrade(upgrade_types::lockdown)) {
		for (auto*c : ghosts) {
			if (c->u->energy < 100) continue;
			if (current_frame - c->last_run <= 15) {
				unit*target = get_best_score(enemy_units, [&](unit*u) {
					if (u->gone) return std::numeric_limits<double>::infinity();
					if (u->visible && !u->detected) return std::numeric_limits<double>::infinity();
					if (!u->type->is_mechanical) return std::numeric_limits<double>::infinity();
					if (u->lockdown_timer) return std::numeric_limits<double>::infinity();
					if (lockdown_target_taken.count(u)) return std::numeric_limits<double>::infinity();
					double value = u->minerals_value + u->gas_value;
					if (value < 200) return std::numeric_limits<double>::infinity();
					double r = diag_distance(u->pos - c->u->pos) - 32 * 8;
					if (r < 0) r = 0;

					return r / value / ((u->shields + u->hp) / (u->stats->shields + u->stats->hp));
				}, std::numeric_limits<double>::infinity());
				if (target) {
					lockdown_target_taken.insert(target);
					double r = units_distance(c->u, target);
					if (r <= 32 * 8) {
						if (current_frame - c->last_used_special >= 8) {
							c->u->game_unit->useTech(upgrade_types::lockdown->game_tech_type, target->game_unit);
							c->last_used_special = current_frame;
							c->u->controller->noorder_until = current_frame + 8;
						}
					} else {
						xy stand_pos;
						xy relpos = c->u->pos - target->pos;
						double ang = atan2(relpos.y, relpos.x);
						stand_pos.x = target->pos.x + (int)(cos(ang) * 32 * 8);
						stand_pos.y = target->pos.y + (int)(sin(ang) * 32 * 8);
						bool can_cloak = players::my_player->has_upgrade(upgrade_types::personal_cloaking) && c->u->energy >= 100 + 25 + 20;
						bool in_attack_range = false;
						bool is_revealed = !can_cloak;
						for (unit*e : enemy_units) {
							if (e->gone) continue;
							test_in_range(c, e, stand_pos, in_attack_range, is_revealed);
							if (in_attack_range && is_revealed) break;
						}
						if (!in_attack_range || !is_revealed) {
							if (in_attack_range && !c->u->cloaked) {
								c->subaction = combat_unit::subaction_use_ability;
								c->ability = upgrade_types::personal_cloaking;
							} else {
								c->subaction = combat_unit::subaction_move;
								c->target_pos = stand_pos;
							}
						}
					}
				}
			}
		}
	}
	if (my_completed_units_of_type[unit_types::nuclear_missile].size() > 0) {
		double nuke_range = players::my_player->has_upgrade(upgrade_types::ocular_implants) ? 32 * 10 : 32 * 8;
		nuke_test.clear();

		a_vector<unit*> nearby_enemies;
		a_vector<unit*> nearby_my_buildings;
		tsc::dynamic_bitset stand_visited((grid::build_grid_width / 2)*(grid::build_grid_height / 2));
		tsc::dynamic_bitset target_visited(grid::build_grid_width*grid::build_grid_height);
		for (auto*c : ghosts) {
			xy best_stand_pos;
			xy best_target_pos;
			double best_score = 0;

			nearby_enemies.clear();
			nearby_my_buildings.clear();
			for (unit*u : enemy_units) {
				if (u->gone) continue;
				if (diag_distance(u->pos - c->u->pos) <= 32 * 25) nearby_enemies.push_back(u);
			}
			for (unit*u : my_buildings) {
				if (diag_distance(u->pos - c->u->pos) <= 32 * 25) nearby_my_buildings.push_back(u);
			}
			
			find_nearby_entirely_walkable_tiles(c->u->pos, [&](xy tile_pos) {
				size_t stand_index = tile_pos.x / 64 + tile_pos.y / 64 * (grid::build_grid_width / 2);
				if (stand_visited.test(stand_index)) return true;
				stand_visited.set(stand_index);
				double d = diag_distance(tile_pos - c->u->pos);
				if (d >= 32 * 8) return false;
				xy stand_pos = tile_pos + xy(16, 16);
				bool in_attack_range = false;
				bool is_revealed = !c->u->cloaked;
				for (unit*e : nearby_enemies) {
					test_in_range(c, e, stand_pos, in_attack_range, is_revealed);
					if (in_attack_range && is_revealed) return false;
				}
				for (double ang = 0.0; ang < PI * 2; ang += PI / 4) {
					xy pos = stand_pos;
					pos.x += (int)(cos(ang)*nuke_range);
					pos.y += (int)(sin(ang)*nuke_range);
					if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) continue;
					size_t target_index = grid::build_square_index(pos);
					if (target_visited.test(target_index)) continue;
					target_visited.set(target_index);
					double score = 0.0;
					for (unit*e : nearby_enemies) {
						// todo: use the actual blast radius of a nuke
						if (diag_distance(e->pos - pos) <= 32 * 7) {
							double damage = std::max(500.0, (e->shields + e->hp) * 2 / 3);
							double mult = 1.0;
							if (!e->type->is_building && e->stats->max_speed>1) mult = 1.0 / e->stats->max_speed;
							if (e->lockdown_timer) mult = 1.0;
							if (e->type->is_worker) mult *= 2;
							if (e->shields + e->hp - damage > 0) mult = damage / (e->stats->shields + e->stats->hp);
							score += (e->minerals_value + e->gas_value)*mult;
						}
					}
					for (unit*e : nearby_my_buildings) {
						// todo: use the actual blast radius of a nuke
						if (diag_distance(e->pos - pos) <= 32 * 7) {
							double damage = std::max(500.0, (e->shields + e->hp) * 2 / 3);
							double mult = 1.0;
							if (e->shields + e->hp - damage > 0) mult = damage / (e->stats->shields + e->stats->hp);
							score -= (e->minerals_value + e->gas_value)*mult;
						}
					}
					nuke_test.emplace_back(pos, score);
					if (score > best_score) {
						best_score = score;
						best_stand_pos = stand_pos;
						best_target_pos = pos;
					}
				}
				return true;
			});
			//log("nuke: best pos is %d %d -> %d %d with score %g\n", best_stand_pos.x, best_stand_pos.y, best_target_pos.x, best_target_pos.y, best_score);
			if (best_score >= 400.0) {
				log("nuking %d %d -> %d %d with score %g\n", best_stand_pos.x, best_stand_pos.y, best_target_pos.x, best_target_pos.y, best_score);
				bool okay = true;
				if (players::my_player->has_upgrade(upgrade_types::personal_cloaking)) {
					if (!c->u->cloaked) {
						c->subaction = combat_unit::subaction_use_ability;
						c->ability = upgrade_types::personal_cloaking;
						okay = false;
					}
				}
				if (okay) {
					if (diag_distance(c->u->pos - best_stand_pos) > 32) {
						c->subaction = combat_unit::subaction_move;
						c->target_pos = best_stand_pos;
					} else {
						c->subaction = combat_unit::subaction_use_ability;
						c->ability = upgrade_types::nuclear_strike;
						c->target_pos = best_target_pos;
						c->last_nuke = current_frame;
					}
				}
			}
		}
	}

	a_vector<combat_unit*> science_vessels;
	for (auto*c : live_combat_units) {
		if (c->u->type == unit_types::science_vessel) science_vessels.push_back(c);
	}
	if (!science_vessels.empty()) {
		a_vector<combat_unit*> wants_defensive_matrix;
		for (auto*c : live_combat_units) {
			if (current_frame - c->last_nuke <= 15 * 14) {
				if (c->u->cloaked) {
					bool in_attack_range = false;
					bool is_revealed = false;
					for (unit*e : enemy_units) {
						if (e->gone) continue;
						test_in_range(c, e, c->u->pos, in_attack_range, is_revealed);
						if (in_attack_range && is_revealed) break;
					}
					if (!is_revealed) continue;
				}
				wants_defensive_matrix.push_back(c);
			}
		}
		a_unordered_set<combat_unit*> target_taken;
		for (auto*c : science_vessels) {
			if (c->u->energy < 100) continue;
			combat_unit*target = get_best_score(wants_defensive_matrix, [&](combat_unit*target) {
				if (target_taken.count(target)) return std::numeric_limits<double>::infinity();
				return diag_distance(c->u->pos - target->u->pos);
			}, std::numeric_limits<double>::infinity());
			if (target) {
				target_taken.insert(target);
				if (current_frame - c->last_used_special >= 8) {
					c->u->game_unit->useTech(upgrade_types::defensive_matrix->game_tech_type, target->u->game_unit);
					c->last_used_special = current_frame;
					c->u->controller->noorder_until = current_frame + 8;
				}
			}
		}
	}

	if (!my_completed_units_of_type[unit_types::scv].empty()) {
		a_vector<combat_unit*> scvs;
		for (auto&g : groups) {
			for (auto*a : g.allies) {
				if (a->u->type == unit_types::scv) scvs.push_back(a);
			}
		}
		a_vector<unit*> wants_repair;
		for (unit*u : my_units) {
			if (!u->is_completed) continue;
			if (!u->type->is_building && !u->type->is_mechanical) continue;
			if (u->controller->action == unit_controller::action_scout) continue;
			if (u->hp < u->stats->hp) wants_repair.push_back(u);
		}
		std::sort(wants_repair.begin(), wants_repair.end(), [&](unit*a, unit*b) {
			return a->minerals_value + a->gas_value > b->minerals_value + b->gas_value;
		});
		for (auto*u : wants_repair) {
			if (scvs.empty()) break;
			for (int i = 0; i < 2; ++i) {
				combat_unit*c = get_best_score(scvs, [&](combat_unit*c) {
					if (c->u == u) return std::numeric_limits<double>::infinity();
					if (!square_pathing::unit_can_reach(c->u, c->u->pos, u->pos)) return std::numeric_limits<double>::infinity();
					return diag_distance(u->pos - c->u->pos);
				}, std::numeric_limits<double>::infinity());
				if (!c) break;
				c->subaction = combat_unit::subaction_repair;
				c->target = u;
			}
		}
	}
}

void do_run(combat_unit*a, const a_vector<unit*>&enemies);

void do_attack(combat_unit*a, const a_vector<unit*>&allies, const a_vector<unit*>&enemies) {
	a->defend_spot = xy();
	a->subaction = combat_unit::subaction_fight;
	if (active_spider_mine_targets.count(a->u)) {
		unit*ne = get_best_score(enemies, [&](unit*e) {
			if (e->is_flying) return std::numeric_limits<double>::infinity();
			return diag_distance(e->pos - a->u->pos);
		});
		a->subaction = combat_unit::subaction_move_directly;
		a->target_pos = ne->pos;
		return;
	}
	for (unit*t : active_spider_mine_targets) {
		if (units_distance(t, a->u) <= 32 * 3) {
			do_run(a, enemies);
			return;
		}
	}
	for (unit*t : my_spider_mines_in_range_of) {
		if (units_distance(t, a->u) <= 32 * 3) {
			do_run(a, enemies);
			return;
		}
	}

	bool wants_to_lay_spider_mines = a->u->spider_mine_count && players::my_player->has_upgrade(upgrade_types::spider_mines);

	unit*u = a->u;
	std::function<std::tuple<double,double,double>(unit*)> score = [&](unit*e) {
		//double ehp = e->shields + e->hp - focus_fire[e];
		//if (ehp <= 0) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		if (!e->visible || !e->detected) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		if (e->invincible) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		weapon_stats*w = e->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
		if (!w) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		double d = units_distance(u, e);
		if (!e->stats->ground_weapon && !e->stats->air_weapon && e->type != unit_types::bunker) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), d);
		//if (!e->stats->ground_weapon && !e->stats->air_weapon) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), d);
		weapon_stats*ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
		if (d < w->min_range) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		double ehp = e->shields + e->hp;
		ehp -= focus_fire[e];
		if (wants_to_lay_spider_mines && !e->type->is_flyer && !e->type->is_hovering && !e->type->is_building) {
			//if (e->type->is_hovering) return std::make_tuple(1000 + d / a->u->stats->max_speed, 0.0, 0.0);
			//return std::make_tuple(prepared_damage[e] + d / a->u->stats->max_speed, 0.0, 0.0);
			ehp += prepared_damage[e] * 4;
		}
		//if (d > w->max_range) return std::make_tuple(std::numeric_limits<double>::infinity(), d, 0.0);
		double hits = ehp / (w->damage*combat_eval::get_damage_type_modifier(w->damage_type, e->type->size));
		//if (d > w->max_range) return std::make_tuple(std::numeric_limits<double>::infinity(), std::ceil(hits), d);
		if (!ew) hits += 4;
		if (e->lockdown_timer) hits += 10;
		//if (d > w->max_range) return std::make_tuple(std::numeric_limits<double>::infinity(), hits + (d - w->max_range) / a->u->stats->max_speed / 90, 0.0);
		if (d > w->max_range) hits += (d - w->max_range) / a->u->stats->max_speed;
		return std::make_tuple(hits, 0.0, 0.0);
	};
	if (a->u->type == unit_types::dropship) {
		score = [&](unit*e) {
			if (e->is_flying) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			double b = e->type == unit_types::siege_tank_siege_mode ? 0.0 : 1.0;
			double d = units_distance(u, e);
			if (dropship_spread[e] == 1) return std::make_tuple(std::numeric_limits<double>::infinity(), b, d);
			return std::make_tuple(0.0, b, d);
		};
	}
	bool targets_allies = false;
	if (a->u->type == unit_types::medic) {
		targets_allies = true;
		score = [&](unit*a) {
			if (!a->type->is_biological) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (is_being_healed[a]) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			double d = units_pathing_distance(u, a);
			return std::make_tuple(a->hp, d, 0.0);
		};
	}
	auto&targets = targets_allies ? allies : enemies;
	unit*target = get_best_score(targets, score);
	//log("u->target is %p, u->order_target is %p\n", u->target, u->order_target);
	if (u->order_target && a->u->type != unit_types::dropship) {
		double new_s = std::get<0>(score(target));
		double old_s = std::get<0>(score(u->order_target));
		//if (old_s < new_s * 2) target = u->order_target;
		//log("old_s %g, new_s %g\n", old_s, new_s);
	}
// 	if (u->target) {
// 		double new_s = std::get<0>(score(target));
// 		double old_s = std::get<0>(score(u->target));
// 		if (old_s < new_s * 2) target = u->target;
// 	}
	//if (target != a->target) log("%s: change target to %p\n", a->u->type->name, target);
	a->target = target;

	if (target) {
		double r = units_distance(a->u, target);
		if (a->u->type == unit_types::dropship) {
			dropship_spread[target] = 1;
			a->target = nullptr;
			a->subaction = combat_unit::subaction_move;
			a->target_pos = target->pos;
			a->target_pos.x += (int)(cos(current_frame / 23.8) * 48);
			a->target_pos.y += (int)(sin(current_frame / 23.8) * 48);
		}

		if (a->u->type == unit_types::medic && r < 64) {
			is_being_healed[target] = true;
		}
	}
	//if (target && (a->u->type == unit_types::marine || a->u->type == unit_types::vulture)) {
	if (target && a->u->type == unit_types::marine) {
		weapon_stats*splash_weapon = nullptr;
		for (unit*e : enemies) {
			weapon_stats*e_weapon = a->u->is_flying ? target->stats->air_weapon : target->stats->ground_weapon;
			if (!e_weapon) continue;
			if (e_weapon->explosion_type != weapon_stats::explosion_type_radial_splash) continue;
			if (units_distance(a->u, e) > e_weapon->max_range) continue;
			splash_weapon = e_weapon;
			break;
		}
		if (splash_weapon) {
			a_vector<unit*> too_close;
			double radius = splash_weapon->outer_splash_radius;
			for (unit*u : allies) {
				double d = (u->pos - a->u->pos).length();
				if (d <= radius) too_close.push_back(u);
			}
			if (too_close.size() > 1) { 
				unit*nearest = get_best_score(too_close, [&](unit*u) {
					return diag_distance(target->pos - u->pos);
				});
				if (nearest != a->u) {
					double avg_dang = 0.0;
					size_t avg_count = 0;
					xy relpos = target->pos - a->u->pos;
					double ang = atan2(relpos.y, relpos.x);
					for (unit*u : allies) {
						double d = (u->pos - a->u->pos).length();
						if (d <= radius) too_close.push_back(u);
						if (d <= 64) {
							xy relpos = target->pos - u->pos;
							double dang = ang - atan2(relpos.y, relpos.x);
							if (dang < -PI) dang += PI * 2;
							if (dang > PI) dang -= PI * 2;
							avg_dang += dang;
							++avg_count;
						}
					}
					avg_dang /= avg_count;
					if (avg_dang < 0) ang += PI / 2;
					else ang -= PI / 2;
					xy npos;
					for (double add = 0.0; add <= PI / 2; add += PI / 8) {
						npos = a->u->pos;
						npos.x += (int)(cos(ang + (avg_dang < 0 ? -add : add)) * 128);
						npos.y += (int)(sin(ang + (avg_dang < 0 ? -add : add)) * 128);
						bool okay = !test_pred(spread_positions, [&](xy p) {
							return (p - npos).length() <= splash_weapon->outer_splash_radius;
						});
						if (okay) break;
						break;
					}
					spread_positions.push_back(npos);
					a->subaction = combat_unit::subaction_move_directly;
					a->target_pos = npos;
				} else {
					double d = units_distance(a->u, target);
					if (splash_weapon->min_range && d>splash_weapon->min_range) {
						a->subaction = combat_unit::subaction_move_directly;
						a->target_pos = target->pos;
					}
				}
			}
		}
	}

	if (target && a->subaction == combat_unit::subaction_fight) {
		weapon_stats*my_weapon = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
		weapon_stats*e_weapon = a->u->is_flying ? target->stats->air_weapon : target->stats->ground_weapon;
		double max_range = 1000.0;
		if (e_weapon && my_weapon) {
			//if (my_weapon->max_range < e_weapon->max_range) max_range = my_weapon->max_range / 2;
			if (e_weapon->min_range) max_range = e_weapon->min_range;
		}
		if (a->u->spider_mine_count && !target->is_flying && !target->type->is_hovering && !target->type->is_building && players::my_player->has_upgrade(upgrade_types::spider_mines)) {
			if (target->hp >= 40) {
				//spider_mine_layers.push_back(a);
				max_range = 0;
			}
		}
		double damage = 0.0;
		if (my_weapon) {
			damage = my_weapon->damage;
			if (target->shields <= 0) damage *= combat_eval::get_damage_type_modifier(my_weapon->damage_type, target->stats->type->size);
			damage -= target->stats->armor;
			if (damage <= 0) damage = 1.0;
			damage *= my_weapon == a->u->stats->ground_weapon ? a->u->stats->ground_weapon_hits : a->u->stats->air_weapon_hits;
			prepared_damage[target] += damage;
		}
		double d = units_distance(target, a->u);
		if (d > max_range) {
			if (a->u->weapon_cooldown > latency_frames) {
				a->subaction = combat_unit::subaction_move_directly;
				a->target_pos = target->pos;
			}
		} else {
			if (my_weapon && d <= my_weapon->max_range && a->u->weapon_cooldown <= latency_frames) {
				focus_fire[target] += damage;
			}
		}
	}

}

template<typename pred_T, typename est_dist_T, typename goal_T>
a_deque<xy> find_path(unit_type*ut, xy from, pred_T&&pred, est_dist_T&&est_dist, goal_T&&goal) {
	if (ut->is_flyer) {
		return flyer_pathing::find_path(from, pred, est_dist, goal);
	} else {
		return square_pathing::find_square_path(square_pathing::get_pathing_map(ut), from, pred, est_dist, goal);
	}
};

tsc::dynamic_bitset run_spot_taken;
void prepare_run() {
	run_spot_taken.resize(grid::build_grid_width*grid::build_grid_height);
	run_spot_taken.reset();
}

void do_run(combat_unit*a, const a_vector<unit*>&enemies) {
	a->defend_spot = xy();
	a->subaction = combat_unit::subaction_move;
	a->target_pos = xy(grid::map_width / 2, grid::map_height / 2);
	unit*nb = get_best_score(my_buildings, [&](unit*u) {
		return diag_distance(a->u->pos - u->pos);
	});
	if (nb) a->target_pos = nb->pos;

	unit*u = a->u;
	
	a_deque<xy> path = find_path(u->type, u->pos, [&](xy pos, xy npos) {
		return true;
	}, [&](xy pos, xy npos) {
		double cost = 0.0;
		for (unit*e : enemies) {
			weapon_stats*w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (e->type == unit_types::bunker) w = units::get_unit_stats(unit_types::marine, e->owner)->ground_weapon;
			if (!w) continue;
			double d = diag_distance(pos - e->pos);
			if (d <= w->max_range*1.5) {
				cost += w->max_range*1.5 - d;
			}
		}
		return cost + diag_distance(a->goal_pos - a->u->pos);
	}, [&](xy pos) {
		//if ((pos - a->u->pos).length() < 128) return false;
		size_t index = grid::build_square_index(pos);
		return entire_threat_area_edge.test(index) && !run_spot_taken.test(index);
	});

	//a->target_pos = best_pos;
	//a->target_pos = square_pathing::get_go_to_along_path(u, path);
	a->target_pos = path.empty() ? a->goal_pos : path.back();
	a->target_pos.x = a->target_pos.x&-32 + 16;
	a->target_pos.y = a->target_pos.y&-32 + 16;

	log("%s: running to spot %g away (%d %d)\n", a->u->type->name, (a->target_pos - a->u->pos).length(), a->target_pos.x, a->target_pos.y);

	int keep_mines = 2;
	if (a->u->hp <= 53) keep_mines = 0;
	if (a->u->spider_mine_count > keep_mines && players::my_player->has_upgrade(upgrade_types::spider_mines) && (a->target_pos - a->u->pos).length() < 32) {
		a->subaction = combat_unit::subaction_use_ability;
		a->ability = upgrade_types::spider_mines;
	}

	run_spot_taken.set(grid::build_square_index(a->target_pos));

	if (a->target && a->target->visible) {
		unit*target = a->target;
		if (a->u->spider_mine_count && !target->is_flying && !target->type->is_hovering && !target->type->is_building && players::my_player->has_upgrade(upgrade_types::spider_mines)) {
			if (target->hp >= 40) {
				//spider_mine_layers.push_back(a);
			}
		}
	}

}

void fight() {

	int process_uid = current_frame;
	prepare_attack();
	prepare_run();

	a_vector<unit*> nearby_enemies, nearby_allies;
	a_vector<combat_unit*> nearby_combat_units;
	std::lock_guard<multitasking::mutex> l(groups_mut);
// 	a_unordered_set<combat_unit*> available;
// 	for (auto&g : groups) {
// 		for (auto*cu : g.allies) available.insert(cu);
// 	}
	for (auto&g : groups) {

		nearby_enemies.clear();
		nearby_combat_units.clear();
		nearby_allies.clear();

		nearby_enemies = g.enemies;
		for (auto*cu : g.allies) {
			double dist = get_best_score_value(nearby_enemies, [&](unit*e) {
				return diag_distance(e->pos - cu->u->pos);
			});
			if (dist <= 32 * 25) {
				nearby_combat_units.push_back(cu);
				nearby_allies.push_back(cu->u);
			}
// 			nearby_combat_units.push_back(cu);
// 			nearby_allies.push_back(cu->u);
		}
		if (!nearby_enemies.empty() && !nearby_allies.empty()) {
// 			nearby_combat_units.clear();
// 			nearby_combat_units.push_back(cu);
// 			nearby_allies.clear();
// 			nearby_allies.push_back(cu->u);
// 			for (auto*a : live_combat_units) {
// 				if (a == cu) continue;
// 				if (diag_distance(cu->u->pos - a->u->pos) <= 32 * 15) {
// 					nearby_combat_units.push_back(a);
// 					nearby_allies.push_back(a->u);
// 				}
// 			}
			bool has_siege_mode = players::my_player->upgrades.count(upgrade_types::siege_mode) != 0;
			auto add = [&](combat_eval::eval&eval, unit*u, int team, bool special) -> combat_eval::combatant& {
				//log("add %s to team %d\n", u->type->name, team);
				if (u->type == unit_types::bunker && u->is_completed) {
					for (int i = 0; i < 4; ++i) {
						eval.add_unit(units::get_unit_stats(unit_types::marine, u->owner), team);
					}
				}
				auto*st = u->stats;
				int cooldown_override = 0;
// 				if (team == 0) {
// 					if (special) {
// 						if (u->type == unit_types::siege_tank_tank_mode && has_siege_mode) {
// 							st = units::get_unit_stats(unit_types::siege_tank_siege_mode, u->owner);
// 							cooldown_override = 120;
// 						}
// 					} else {
// 						if (u->type == unit_types::siege_tank_siege_mode) {
// 							st = units::get_unit_stats(unit_types::siege_tank_tank_mode, u->owner);
// 							cooldown_override = 120;
// 						}
// 					}
// 				}
				if (u->game_order == BWAPI::Orders::Sieging || u->game_order == BWAPI::Orders::Unsieging) {
					//cooldown_override = u->game_unit->getOrderTimer();
					//cooldown_override = 45;
				}
				if (!u->visible) cooldown_override = 0;
				auto&c = eval.add_unit(st, team);
				c.move = get_best_score(make_transform_filter(team ? nearby_allies : nearby_enemies, [&](unit*e) {
					if (e->type->is_flyer && !c.st->ground_weapon) return std::numeric_limits<double>::infinity();
					return units_distance(e, u);
				}), identity<double>());
				if (c.move == std::numeric_limits<double>::infinity()) c.move = 32 * 8;
				eval.set_unit_stuff(c, u);
				if (cooldown_override > c.cooldown) c.cooldown = cooldown_override;
				//log("added %s to team %d -- move %g, shields %g, hp %g, cooldown %d\n", st->type->name, team, c.move, c.shields, c.hp, c.cooldown);
				return c;
			};
			int eval_frames = 15 * 60;
			combat_eval::eval eval;
			eval.max_frames = eval_frames;
			for (unit*a : nearby_allies) add(eval, a, 0, false);
			for (unit*e : nearby_enemies) add(eval, e, 1, false);
			eval.run();

			combat_eval::eval ground_eval;
			ground_eval.max_frames = eval_frames;
			for (unit*a : nearby_allies) {
				if (!a->is_flying) add(ground_eval, a, 0, false);
			}
			for (unit*e : nearby_enemies) add(ground_eval, e, 1, false);
			ground_eval.run();

			//bool ground_fight = (ground_eval.teams[0].start_supply - ground_eval.teams[0].end_supply) < (ground_eval.teams[1].start_supply - ground_eval.teams[1].end_supply);
			bool ground_fight = ground_eval.teams[0].score > ground_eval.teams[1].score;

			combat_eval::eval air_eval;
			air_eval.max_frames = eval_frames;
			for (unit*a : nearby_allies) {
				if (a->is_flying) add(air_eval, a, 0, false);
			}
			for (unit*e : nearby_enemies) add(air_eval, e, 1, false);
			air_eval.run();

			//bool air_fight = (air_eval.teams[0].start_supply - air_eval.teams[0].end_supply) < (air_eval.teams[1].start_supply - air_eval.teams[1].end_supply);
			bool air_fight = ground_eval.teams[0].score > ground_eval.teams[1].score;

			bool has_siege_tanks = test_pred(nearby_allies, [&](unit*u) {
				return u->type == unit_types::siege_tank_siege_mode || u->type == unit_types::siege_tank_tank_mode;
			});
			bool is_sp = false;
			if (false) {
				combat_eval::eval sp_eval;
				sp_eval.max_frames = eval_frames;
				for (unit*a : nearby_allies) add(sp_eval, a, 0, true);
				for (unit*e : nearby_enemies) add(sp_eval, e, 1, false);
				sp_eval.run();

				log("sp result: supply %g %g  damage %g %g  in %d frames\n", sp_eval.teams[0].end_supply, sp_eval.teams[1].end_supply, sp_eval.teams[0].damage_dealt, sp_eval.teams[1].damage_dealt, sp_eval.total_frames);

				double reg_score = eval.teams[0].score - eval.teams[1].score;
				double sp_score = sp_eval.teams[0].score - sp_eval.teams[1].score;
				log("sp: score %g vs %g\n", sp_score, reg_score);
				if (sp_score > reg_score + 50) {
					eval = sp_eval;
					is_sp = true;
				}
			}
			bool has_dropship = false;
			for (unit*u : nearby_allies) {
				if (u->type == unit_types::dropship) has_dropship = true;
			}
			bool is_drop = false;
			a_map<unit*, unit*> loaded_units;
			a_map<unit*, int> dropships;
			bool some_are_not_loaded_yet = false;
			if (has_dropship && false) {
				combat_eval::eval sp_eval;
				sp_eval.max_frames = eval_frames;
				for (unit*u : nearby_allies) {
					if (u->type == unit_types::dropship) {
						int pickup_time = 0;
						int space = u->type->space_provided;
						for (unit*lu : u->loaded_units) {
							loaded_units[lu] = u;
							space -= lu->type->space_required;
						}
						while (space > 0) {
							unit*nu = get_best_score(nearby_allies, [&](unit*nu) {
								if (nu->type->is_flyer) return std::numeric_limits<double>::infinity();
								if (nu->is_loaded || loaded_units.find(nu) != loaded_units.end()) return std::numeric_limits<double>::infinity();
								if (diag_distance(nu->pos - u->pos) > 32 * 10) return std::numeric_limits<double>::infinity();
								if (nu->type->space_required > space) return std::numeric_limits<double>::infinity();
								return diag_distance(nu->pos - u->pos);
							}, std::numeric_limits<double>::infinity());
							if (!nu) break;
							loaded_units[nu] = u;
							space -= nu->type->space_required;
							//pickup_time += (int)(diag_distance(nu->pos - u->pos) / u->stats->max_speed);
							some_are_not_loaded_yet = true;
						}
						dropships[u] = pickup_time;
					}
				}
				for (unit*a : nearby_allies) {
					if (loaded_units.find(a) != loaded_units.end()) continue;
					auto&c = add(sp_eval, a, 0, false);
					if (dropships.find(a) != dropships.end()) {
						c.force_target = true;
						dropships[a] += (int)(c.move / a->stats->max_speed);
					}
				}
				a_unordered_map<unit*,int> loaded_unit_count;
				for (auto&v : loaded_units) {
					auto&c = add(sp_eval, std::get<0>(v), 0, false);
					c.move = -1000;
					c.loaded_until = dropships[std::get<1>(v)] + (loaded_unit_count[std::get<1>(v)]++ * 15);
					//c.loaded_until = dropships[std::get<1>(v)];
				}
				for (unit*e : nearby_enemies) add(sp_eval, e, 1, false);
				sp_eval.run();

				log("drop result: supply %g %g  damage %g %g  in %d frames\n", sp_eval.teams[0].end_supply, sp_eval.teams[1].end_supply, sp_eval.teams[0].damage_dealt, sp_eval.teams[1].damage_dealt, sp_eval.total_frames);

				//if (sp_eval.teams[0].damage_dealt > eval.teams[0].damage_dealt) {
				if (sp_eval.teams[1].damage_dealt < eval.teams[1].damage_dealt) {
					eval = sp_eval;
					is_drop = true;
				}
			}

			{
				a_map<unit_type*, int> my_count, op_count;
				for (unit*a : nearby_allies) ++my_count[a->type];
				for (unit*e : nearby_enemies) ++op_count[e->type];
				log("combat::\n");
				log("my units -");
				for (auto&v : my_count) log(" %dx%s", v.second, short_type_name(v.first));
				log("\n");
				log("op units -");
				for (auto&v : op_count) log(" %dx%s", v.second, short_type_name(v.first));
				log("\n");

				log("result: supply %g %g  damage %g %g  in %d frames\n", eval.teams[0].end_supply, eval.teams[1].end_supply, eval.teams[0].damage_dealt, eval.teams[1].damage_dealt, eval.total_frames);
			}

			//bool fight = eval.teams[0].damage_dealt > eval.teams[1].damage_dealt*0.75;
			double fact = 1.0;
			//if (current_frame - cu->last_fight <= 40) fact = 0.5;
			bool already_fighting = test_pred(nearby_combat_units, [&](combat_unit*cu) {
				return current_frame - cu->last_fight <= 60 && current_frame - cu->last_run > 60;
			});
			if (already_fighting) fact = 0.5;
			//bool fight = eval.teams[0].end_supply > eval.teams[1].end_supply * fact;
// 			double my_killed = eval.teams[1].start_supply - eval.teams[1].end_supply;
// 			double op_killed = eval.teams[0].start_supply - eval.teams[0].end_supply;
// 			bool fight = my_killed > op_killed*fact;
// 			//bool fight = false;
// 			//fight |= eval.teams[0].end_supply > eval.teams[1].end_supply*fact;
// 			//fight |= eval.teams[0].score > eval.teams[1].score*fact;
// 			//fight |= eval.teams[0].damage_dealt > eval.teams[1].damage_dealt*fact;
// 			fight |= eval.teams[1].end_supply == 0;
// 			fight &= eval.teams[0].end_supply >= 1;
			bool fight = eval.teams[0].score >= eval.teams[1].score;
			if ((air_fight || ground_fight) && !is_drop) fight = false;
			else {
				if (fight) log("fight!\n");
				else log("run!\n");
			}
			if (ground_fight) log("ground fight!\n");
			if (air_fight) log("air fight!\n");

			bool quick_fight = false;
// 			if (!fight) {
// 				combat_eval::eval quick_eval;
// 				quick_eval.max_frames = 15 * 2;
// 				for (unit*a : nearby_allies) add(quick_eval, a, 0, false);
// 				for (unit*e : nearby_enemies) add(quick_eval, e, 1, false);
// 				quick_eval.run();
// 				log("quick result: supply %g %g  damage %g %g  in %d frames\n", quick_eval.teams[0].end_supply, quick_eval.teams[1].end_supply, quick_eval.teams[0].damage_dealt, quick_eval.teams[1].damage_dealt, quick_eval.total_frames);
// 				if (quick_eval.teams[0].damage_dealt > quick_eval.teams[1].damage_dealt*fact) {
// 					log("quick fight!\n");
// 					fight = true;
// 					quick_fight = true;
// 				}
// 			}

			unit*defensive_matrix_target = nullptr;
			if (!fight && !ground_fight && !air_fight) {
				bool has_defensive_matrix = false;
				for (unit*a : nearby_allies) {
					if (a->type == unit_types::science_vessel && a->energy >= 100) {
						has_defensive_matrix = true;
						break;
					}
				}
				if (has_defensive_matrix) {
					combat_eval::eval sp_eval;
					sp_eval.max_frames = eval_frames;
					double lowest_move = std::numeric_limits<double>::infinity();
					unit*target = nullptr;
					size_t target_idx = 0;
					size_t idx = 0;
					for (unit*a : nearby_allies) {
						auto&c = add(sp_eval, a, 0, true);
						if (c.move < lowest_move) {
							lowest_move = c.move;
							target = a;
							target_idx = idx;
						}
						++idx;
					}
					for (unit*e : nearby_enemies) add(sp_eval, e, 1, false);
					if (target) {
						sp_eval.teams[0].units[target_idx].hp += 250;
						sp_eval.run();

						log("defensive matrix result: supply %g %g  damage %g %g  in %d frames\n", sp_eval.teams[0].end_supply, sp_eval.teams[1].end_supply, sp_eval.teams[0].damage_dealt, sp_eval.teams[1].damage_dealt, sp_eval.total_frames);

// 						double my_killed = sp_eval.teams[1].start_supply - sp_eval.teams[1].end_supply;
// 						double op_killed = sp_eval.teams[0].start_supply - sp_eval.teams[0].end_supply;
// 						bool fight = my_killed > op_killed*fact;
// 						fight |= sp_eval.teams[1].end_supply == 0;
// 						fight &= sp_eval.teams[0].end_supply >= 1;
						bool fight = sp_eval.teams[0].score >= sp_eval.teams[1].score;
						if (fight) {
							defensive_matrix_target = target;
						}
					}
				}
			}

			bool ignore = false;
			//if (eval.teams[1].damage_dealt < eval.teams[0].damage_dealt / 10) {
// 			if (eval.teams[1].damage_dealt == 0) {
// 				if (diag_distance(cu->u->pos - cu->goal_pos) >= 32 * 15) {
// 					if (eval.total_frames > 15 * 15) {
// 						ignore = true;
// 					}
// 				}
// 			}
			size_t gone_count = 0; // .... i probably meant invisible_count
			for (unit*e : nearby_enemies) {
				if (e->gone) ++gone_count;
			}
			size_t my_siege_tank_count = 0;
			size_t my_sieged_tank_count = 0;
			size_t my_unsieged_tank_count = 0;
			for (unit*u : nearby_allies) {
				if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) ++my_siege_tank_count;
				if (u->type == unit_types::siege_tank_tank_mode) ++my_unsieged_tank_count;
				if (u->type == unit_types::siege_tank_siege_mode) ++my_sieged_tank_count;
			}
			size_t max_sieged_tanks = my_siege_tank_count * (nearby_enemies.size() - gone_count / 2) / nearby_enemies.size();
			size_t max_siege_count = max_sieged_tanks;
			size_t max_unsiege_count = my_siege_tank_count - max_sieged_tanks;
			size_t siege_count = 0;
			size_t unsiege_count = 0;
			for (unit*u : nearby_allies) {
				if (u->type == unit_types::siege_tank_siege_mode) {
					--max_siege_count;
				}
			}

// 			bool some_are_unloading = false;
// 			for (auto&v : dropships) {
// 				some_are_unloading |= v.second <= 15 * 3;
// 			}
			bool some_are_attacking = false;
			for (unit*u : nearby_allies) {
				if (current_frame - u->last_attacked < 60) some_are_attacking = true;
			}

			unit*defensive_matrix_vessel = nullptr;
			if (defensive_matrix_target) {
				defensive_matrix_vessel = get_best_score(nearby_allies, [&](unit*u) {
					if (u->type != unit_types::science_vessel || u->energy < 100) return std::numeric_limits<double>::infinity();
					return diag_distance(defensive_matrix_target->pos - u->pos);
				}, std::numeric_limits<double>::infinity());
			}

			if (!ignore) {
				for (auto*a : nearby_combat_units) {
					if (!quick_fight) a->last_fight = current_frame;
					a->last_processed_fight = current_frame;
					bool attack = fight;
					attack |= a->u->is_flying && air_fight;
					attack |= !a->u->is_flying && ground_fight;
					//if (my_sieged_tank_count>my_unsieged_tank_count)
					if (attack) {
						/*if (is_sp) {
							if (a->u->type == unit_types::siege_tank_tank_mode && has_siege_mode) {
								if (siege_count < max_siege_count) {
									++siege_count;
									if (current_frame >= a->u->controller->noorder_until && a->u->game_unit->siege()) {
										a->u->controller->noorder_until = current_frame + 15 * 8;
									}
								}
							}
							if (a->u->type == unit_types::siege_tank_siege_mode && unsiege_count < max_sieged_tanks) {
								if (current_frame - a->u->last_attacked >= 120) {
									++unsiege_count;
									if (a->u->game_unit->unsiege() && current_frame >= a->u->controller->noorder_until) {
										a->u->controller->noorder_until = current_frame + 8;
									}
								}
							}
						} else */
						if (a->u->type == unit_types::siege_tank_siege_mode) {
							if (current_frame - a->u->last_attacked >= 120 && current_frame>=a->u->controller->noorder_until) {
								if (++a->unsiege_counter >= 30) {
									if (a->u->game_unit->unsiege()) {
										a->u->controller->noorder_until = current_frame + 8;
										a->unsiege_counter = 0;
									}
								}
							}
						}
						if (a->u->type != unit_types::siege_tank_siege_mode) a->unsiege_counter = 0;
						bool dont_attack = false;
						/*bool unload = true;
						if (is_drop) {
							auto lui = loaded_units.find(a->u);
							if (lui != loaded_units.end()) {
								if (dropships[lui->second] > 15 * 1) {
									a->subaction = combat_unit::subaction_move;
									a->target_pos = lui->second->pos;
									dont_attack = true;
								}
							}
							auto di = dropships.find(a->u);
							if (di != dropships.end()) {
								unload = di->second <= 15 * 1;
								if (!unload) {
									bool loaded_any = false;
									for (auto&v : loaded_units) {
										if (v.second == a->u) {
											unit*lu = v.first;
											if (!lu->is_loaded) {
												a->subaction = combat_unit::subaction_move;
												a->target_pos = lu->pos;
												if (diag_distance(lu->pos - a->u->pos) <= 32 * 4) {
													a->subaction = combat_unit::subaction_idle;
													a->u->controller->action = unit_controller::action_idle;
													if (current_frame >= a->u->controller->noorder_until) {
														loaded_any = true;
														lu->game_unit->rightClick(a->u->game_unit);
														//a->u->game_unit->load(lu->game_unit);
														lu->controller->noorder_until = current_frame + 15;
													}
												}
												dont_attack = true;
											}
										}
									}
									if (loaded_any) a->u->controller->noorder_until = current_frame + 4;
								}
							}
						}
						if (!a->u->loaded_units.empty() && unload) {
							dont_attack = true;
							a->subaction = combat_unit::subaction_idle;
							a->u->controller->action = unit_controller::action_idle;
							if (current_frame >= a->u->controller->noorder_until) {
								a->u->game_unit->unload(a->u->loaded_units.front()->game_unit);
								a->u->controller->noorder_until = current_frame + 4;
							}
						}*/
						
						if (!dont_attack) {
							if (a->u->type == unit_types::dropship && a->u->loaded_units.empty()) {
								//do_run(a, nearby_enemies);
								//dont_attack = true;
							}
						}
						if (!dont_attack) {
							if (is_drop && some_are_not_loaded_yet && !some_are_attacking) do_run(a, nearby_enemies);
							else do_attack(a, nearby_allies, nearby_enemies);
						}
					} else {
						a->last_run = current_frame;
// 						if (a->u->type == unit_types::siege_tank_siege_mode) {
// 							if (current_frame - a->u->last_attacked >= 120 && current_frame >= a->u->controller->noorder_until) {
// 								if (a->u->game_unit->unsiege()) {
// 									a->u->controller->noorder_until = current_frame + 8;
// 								}
// 							} else do_attack(a, nearby_allies, nearby_enemies, process_uid);
// 						} else do_run(a, nearby_enemies);
						bool run = true;
						if (a->u == defensive_matrix_vessel) {
							a->subaction = combat_unit::subaction_use_ability;
							a->ability = upgrade_types::defensive_matrix;
							a->target = defensive_matrix_target;
							run = false;
						}
						if (run) do_run(a, nearby_enemies);
					}
					multitasking::yield_point();
				}
			} else {
				for (auto*a : nearby_combat_units) {
					a->last_processed_fight = current_frame;
				}
			}

		} else {
			for (auto*a : nearby_combat_units) {
				//visited.insert(a);
			}
// 			cu->action = combat_unit::action_idle;

// 			cu->subaction = combat_unit::subaction_move;
// 			cu->target_pos = cu->u->pos;
		}
	}

	finish_attack();

}

void execute() {

	for (auto*cu : live_combat_units) {

// 		if (cu->subaction == combat_unit::subaction_idle) {
// 			cu->u->controller->action = unit_controller::action_move;
// 			cu->u->controller->go_to = cu->defend_spot;
// 		}

		if (cu->subaction == combat_unit::subaction_fight) {
			//cu->u->controller->action = unit_controller::action_fight;
			cu->u->controller->action = unit_controller::action_attack;
			cu->u->controller->target = cu->target;
		}
		if (cu->subaction == combat_unit::subaction_move) {
			cu->u->controller->action = unit_controller::action_move;
			cu->u->controller->go_to = cu->target_pos;
		}
		if (cu->subaction == combat_unit::subaction_move_directly) {
			cu->u->controller->action = unit_controller::action_move_directly;
			cu->u->controller->go_to = cu->target_pos;
		}
		if (cu->subaction == combat_unit::subaction_use_ability) {
			cu->u->controller->action = unit_controller::action_use_ability;
			cu->u->controller->ability = cu->ability;
			cu->u->controller->target = cu->target;
			cu->u->controller->target_pos = cu->target_pos;
		}
		if (cu->subaction == combat_unit::subaction_repair) {
			cu->u->controller->action = unit_controller::action_repair;
			cu->u->controller->target = cu->target;
		}

		if (cu->subaction == combat_unit::subaction_idle) {
			if (cu->u->controller->action == unit_controller::action_attack) {
				cu->u->controller->action = unit_controller::action_idle;
			}
		}

	}

}

void combat_task() {

	int last_update_bases = 0;

	while (true) {

		if (current_frame - last_update_bases >= 30) {
			last_update_bases = current_frame;

			update_my_base();
			update_op_base();
		}

		update_combat_units();

		fight();

		defence();

		execute();

		multitasking::sleep(1);
	}

}

#include "combat_tactics.h"

void update_combat_groups_task() {
	while (true) {

		multitasking::sleep(10);

		update_groups();

		combat_tactics::on_groups_updated();

	}
}

void combat_tactics_task() {
	while (true) {

		multitasking::sleep(1);

		combat_tactics::run();

	}
}

void render() {

// 	BWAPI::Position screen_pos = game->getScreenPosition();
// 	for (int y = screen_pos.y; y < screen_pos.y + 400; y += 32) {
// 		for (int x = screen_pos.x; x < screen_pos.x + 640; x += 32) {
// 			if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;
// 
// 			x &= -32;
// 			y &= -32;
// 
// 			size_t index = grid::build_square_index(xy(x, y));
// 			if (!my_base.test(index)) {
// 				game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Colors::Green);
// 			}
// 
// 		}
// 	}
	
	for (auto&ds : defence_spots) {
		xy pos = ds.pos;
		pos.x &= -32;
		pos.y &= -32;
		game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Colors::Red);
	}

	for (auto&g : groups) {
		xy pos = g.enemies.front()->pos;
		for (unit*e : g.enemies) {
			game->drawLineMap(e->pos.x, e->pos.y, pos.x, pos.y, BWAPI::Colors::Red);
		}
		for (auto*cu : g.allies) {
			game->drawLineMap(cu->u->pos.x, cu->u->pos.y, pos.x, pos.y, BWAPI::Colors::Green);
		}
	}

	xy screen_pos = bwapi_pos(game->getScreenPosition());
	for (auto&g : groups) {
		for (size_t idx : g.threat_area) {
			xy pos((idx % (size_t)grid::build_grid_width) * 32, (idx / (size_t)grid::build_grid_width) * 32);
			if (pos.x < screen_pos.x || pos.y < screen_pos.y) continue;
			if (pos.x + 32 >= screen_pos.x + 640 || pos.y + 32 >= screen_pos.y + 400) continue;
			
			game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Colors::Brown, false);
		}
	}

	for (auto&v : nuke_test) {
		xy pos = std::get<0>(v);
		double val = std::get<1>(v);
		game->drawTextMap(pos.x, pos.y, "%g", val);
	}
}

void init() {

	my_base.resize(grid::build_grid_width*grid::build_grid_height);
	op_base.resize(grid::build_grid_width*grid::build_grid_height);

	entire_threat_area.resize(grid::build_grid_width*grid::build_grid_height);
	entire_threat_area_edge.resize(grid::build_grid_width*grid::build_grid_height);

	multitasking::spawn(generate_defence_spots_task, "generate defence spots");
	multitasking::spawn(combat_task, "combat");
	multitasking::spawn(update_combat_groups_task, "update combat groups");
	multitasking::spawn(combat_tactics_task, "combat tactics");

	render::add(render);

	combat_tactics::init();

}

}
