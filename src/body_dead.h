#ifndef _BODY_DEAD_H_
#define _BODY_DEAD_H_

/* One is "mostly" dead when one makes a jump and slams into a wall.
One is "all" dead when one is crushed by a bad guy or whatever.
The main difference between each kind of death is where you
will respawn: when mostly dead, you respawn from where you jumped,
when all dead, you respawn at last checkpoint. */
enum body_dead {
    BODY_NOT_DEAD,
    BODY_MOSTLY_DEAD,
    BODY_ALL_DEAD
};

#endif