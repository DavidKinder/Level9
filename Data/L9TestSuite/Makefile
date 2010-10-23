all: src/l9test
	mkdir -p out
	$(call run-test,colossal_adventure_v1)
	$(call run-test,colossal_adventure_v2)
	$(call run-test,colossal_adventure_v3)
	$(call run-test,adventure_quest_v1)
	$(call run-test,adventure_quest_v2)
	$(call run-test,adventure_quest_v3)
	$(call run-test,dungeon_adventure_v1)
	$(call run-test,dungeon_adventure_v2)
	$(call run-test,dungeon_adventure_v3)
	$(call run-test,snowball_v1)
	$(call run-test,snowball_v2)
	$(call run-test,snowball_v3)
	$(call run-test,lords_of_time_v1)
	$(call run-test,lords_of_time_v2)
	$(call run-test,return_to_eden_v2)
	$(call run-test,return_to_eden_v3)
	$(call run-test,erik_the_viking_v2)
	$(call run-test,emerald_isle_v2)
	$(call run-test,worm_in_paradise_v3)
	$(call run-test,red_moon_v2)
	$(call run-test,price_of_magik_v3)
	$(call run-test,time_and_magik_v4_1)
	$(call run-test,time_and_magik_v4_2)
	$(call run-test,time_and_magik_v4_3)
	$(call run-test,knight_orc_v4_1)
	$(call run-test,gnome_ranger_v4_1)
	$(call run-test,lancelot_v4_1)
	$(call run-test,ingrids_back_v4_1)
	$(call run-test,scapeghost_v4_1)

define run-test
	rm -f out/$(1).txt
	src/l9test dat/$(1).dat scripts/$(1).txt >out/$(1).txt
	diff -q --strip-trailing-cr ref/$(1).ref out/$(1).txt
endef

clean:
	rm -f src/*.exe src/*.o
	rm -rf out

src/l9test: src/l9test.o src/level9.o

CFLAGS = -I../..

src/level9.o: ../../level9.c
	$(CC) $(CFLAGS) -c -o $@ $^
src/l9test.o: src/l9test.c
	$(CC) $(CFLAGS) -c -o $@ $^

