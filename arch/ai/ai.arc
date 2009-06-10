Object ai_generic
name Generic AI
msg
# This is a generic AI that can be used for most mobs
processes:
look_for_other_mobs
friendship
choose_enemy

moves: 
# stand_still
sleep
# run_away_from_enemy hp_threshold=6
move_towards_enemy
move_towards_enemy_last_known_pos
search_for_lost_enemy
move_towards_waypoint
# move_randomly xlimit=5 ylimit=5
move_towards_home

reaction_moves:

actions:
melee_attack_enemy
bow_attack_enemy
spell_attack_enemy
endmsg
face ai.101
sys_object 1
type 126
end
