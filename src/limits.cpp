/***************************************************************************
 *  file: limits.c , Limit and gain control module.        Part of DIKUMUD *
 *  Usage: Procedures controling gain and limit.                           *
 *  Copyright (C) 1990, 1991 - see 'license.doc' for complete information. *
 *                                                                         *
 *  Copyright (C) 1992, 1993 Michael Chastain, Michael Quan, Mitchell Tse  *
 *  Performance optimization and bug fixes by MERC Industries.             *
 *  You can use our stuff in any way you like whatsoever so long as ths   *
 *  copyright notice remains intact.  If you like it please drop a line    *
 *  to mec@garnet.berkeley.edu.                                            *
 *                                                                         *
 *  This is free software and you are benefitting.  We hope that you       *
 *  share your changes too.  What goes around, comes around.               *
 ***************************************************************************/
/* $Id: limits.cpp,v 1.1 2002/06/13 04:32:18 dcastle Exp $ */

extern "C"
{
#include <stdio.h>
#include <string.h>
}
#ifdef LEAK_CHECK
#include <dmalloc.h>
#endif

#ifdef BANDWIDTH
  #include <bandwidth.h>
#endif
#include <room.h>
#include <character.h>
#include <utility.h>
#include <mobile.h>
#include <isr.h>
#include <spells.h> // TYPE
#include <levels.h>
#include <player.h>
#include <obj.h>
#include <connect.h>
#include <db.h> // exp_table
#include <fight.h> // damage
#include <ki.h>
#include <game_portal.h>
#include <act.h>
#include <handler.h>
#include <race.h>
#include <returnvals.h>

extern CHAR_DATA *character_list;
extern struct obj_data *object_list;
extern CWorld world;

/* External procedures */

void update_pos( CHAR_DATA *victim );                 /* in fight.c */
struct time_info_data age(CHAR_DATA *ch);
void affect_modify(CHAR_DATA *ch, byte loc,
		byte mod, long bitv, bool add);

/* When age < 15 return the value p0 */
/* When age in 15..29 calculate the line between p1 & p2 */
/* When age in 30..44 calculate the line between p2 & p3 */
/* When age in 45..59 calculate the line between p3 & p4 */
/* When age in 60..79 calculate the line between p4 & p5 */
/* When age >= 80 return the value p6 */
int graf(int age, int p0, int p1, int p2, int p3, int p4, int p5, int p6)
{

    if (age < 15)
	return(p0);                               /* < 15   */
    else if (age <= 29) 
	return (int) (p1+(((age-15)*(p2-p1))/15));  /* 15..29 */
    else if (age <= 44)
	return (int) (p2+(((age-30)*(p3-p2))/15));  /* 30..44 */
    else if (age <= 59)
	return (int) (p3+(((age-45)*(p4-p3))/15));  /* 45..59 */
    else if (age <= 79)
	return (int) (p4+(((age-60)*(p5-p4))/20));  /* 60..79 */
    else
	return(p6);                               /* >= 80 */
}

/* The three MAX functions define a characters Effective maximum */
/* Which is NOT the same as the ch->max_xxxx !!!          */
long mana_limit(CHAR_DATA *ch)
{
    int max;

    if (!IS_NPC(ch))
      max = (ch->max_mana);
    else
      max = 100;
    
    return(max);
}

long ki_limit(CHAR_DATA *ch)
{
	if(!IS_NPC(ch))
	    return(ch->max_ki);
	else
	    return(0);
}

long hit_limit(CHAR_DATA *ch)
{
    int max;

    if (!IS_NPC(ch))
      max = (ch->max_hit) +
	(graf(age(ch).year, 2,4,17,14,8,4,3));
    else 
      max = (ch->max_hit);


/* Class/Level calculations */

/* Skill/Spell calculations */
    
  return (max);
}


long move_limit(CHAR_DATA *ch)
{
    int max;

    if (!IS_NPC(ch))
	/* HERE SHOULD BE CON CALCULATIONS INSTEAD */
	max = (ch->max_move) + 
	  graf(age(ch).year, 50,70,160,120,100,40,20);
    else
	max = ch->max_move;

/* Class/Level calculations */

/* Skill/Spell calculations */

  return (max);
}




/* manapoint gain pr. game hour */
int mana_gain(CHAR_DATA *ch)
{
  int gain;
  int divisor = 100000;
  int modifier;
  
  if(IS_NPC(ch))
    gain = GET_LEVEL(ch);
  else {
    gain = graf(age(ch).year, 2,3,4,6,7,8,9);

    switch (GET_POS(ch)) {
      case POSITION_SLEEPING: divisor = 1; break;
      case POSITION_RESTING:  divisor = 2; break;
      case POSITION_SITTING:  divisor = 2; break;
      default:                divisor = 4; break;
    }
    
    if(GET_CLASS(ch) == CLASS_MAGIC_USER ||
       GET_CLASS(ch) == CLASS_ANTI_PAL)
      modifier = GET_INT(ch);
    else
      modifier = GET_WIS(ch);
    
    gain += (int)(modifier / divisor);

    // int bonus modifier.  1.1 at 16int/wis up to 2.5 at 30int/wis
    if(modifier > 15)
      gain = (int)(gain * (((float)modifier - 5.0) / 10.0));
  }

  gain += ch->mana_regen;

  if (IS_AFFECTED(ch, AFF_POISON))
    gain >>= 2;

  if((GET_COND(ch,FULL)==0)||(GET_COND(ch,THIRST)==0))
    gain >>= 2;
 
  return (gain);
}


/* Hitpoint gain pr. game hour */
int hit_gain(CHAR_DATA *ch)
{
  int gain = 1;
  int divisor = 8;
  struct affected_type * af;

  /* Neat and fast */
  if(IS_NPC(ch))
    gain = (GET_MAX_HIT(ch) / 30);
  
  /* PC's */
  else {
    gain = GET_MAX_HIT(ch) / 160;

    /* Position calculations    */
    switch (GET_POS(ch)) {
      case POSITION_SLEEPING: divisor = 1; break;
      case POSITION_RESTING:  divisor = 2; break;
      case POSITION_SITTING:  divisor = 4; break;
      default:                divisor = 16; break;
    }

    if(gain < 1) 
      gain = 1;

    gain /= divisor;    
    gain += (GET_CON(ch)/2);

    if((af = affected_by_spell(ch, SPELL_RAPID_MEND)))
      gain += af->modifier;

    // con multiplier modifier 15 = 1.0  30 = 1.45 (.03 increments)
    if(GET_CON(ch) > 15)
      gain = (int)(gain * ((float)1+ (.03 * (GET_CON(ch) - 15.0))));

    if(GET_CLASS(ch) == CLASS_MAGIC_USER || GET_CLASS(ch) == CLASS_CLERIC || GET_CLASS(ch) == CLASS_DRUID)
      gain = (int)((float)gain * 0.7);
  }

  gain += ch->hit_regen;

  if(IS_AFFECTED(ch, AFF_POISON))
    gain >>= 2;

  if((GET_COND(ch, FULL)==0) || (GET_COND(ch, THIRST)==0))
    gain >>= 2;

  return (gain);
}


int ki_gain_level(CHAR_DATA *ch)
{
	if(IS_NPC(ch))
		return 0;

	/* Monks gain one point /level */
	if(GET_CLASS(ch) == CLASS_MONK)
		return 1;

	/* If they're stll here they're not a monk */
	/* Other chars gain one point every other level */
	if(GET_LEVEL(ch) % 2)
		return 1;
	return 0;
}

int move_gain(CHAR_DATA *ch)
/* move gain pr. game hour */
{
    int gain;
    int divisor = 100000;
    struct affected_type * af;

    if(IS_NPC(ch)) {
	return(GET_LEVEL(ch));  
	/* Neat and fast */
    } else {
	gain = graf(age(ch).year, 4,5,6,7,4,3,2);

	switch (GET_POS(ch)) {
	    case POSITION_SLEEPING: divisor = 1; break;
	    case POSITION_RESTING:  divisor = 2; break;
	    default:                divisor = 4; break;
	}
	gain += (GET_CON(ch) + GET_DEX(ch)) / divisor;

        if((af = affected_by_spell(ch, SPELL_RAPID_MEND)))
          gain += (int)(af->modifier * 1.5);
    }

    gain += ch->move_regen;

    if (IS_AFFECTED(ch,AFF_POISON))
	gain >>= 2;

    if((GET_COND(ch,FULL)==0)||(GET_COND(ch,THIRST)==0))
	gain >>= 2;

    return (gain);
}

void redo_hitpoints( CHAR_DATA *ch)
{
  extern struct con_app_type con_app[];
  /*struct affected_type *af;*/
  int i, j, bonus;

  ch->max_hit = ch->raw_hit;
  bonus = (GET_LEVEL(ch)) * (con_app[GET_CON(ch)].hitp);
 
  if ((GET_CLASS(ch) == CLASS_MAGIC_USER) ||
     (GET_CLASS(ch) == CLASS_CLERIC))
    bonus /= 2;

  ch->max_hit += bonus;

  for (i=0; i<MAX_WEAR; i++) 
  {
    if (ch->equipment[i])
      for (j=0; j<ch->equipment[i]->num_affects; j++) 
      {
        if (ch->equipment[i]->affected[j].location == APPLY_HIT)
          affect_modify(ch, ch->equipment[i]->affected[j].location,
                           ch->equipment[i]->affected[j].modifier,
                           0, TRUE);
      }
  }
}


void redo_mana ( CHAR_DATA *ch)

{
  extern int mana_bonus[];
   /*struct affected_type *af;*/
   int i, j, bonus;

  ch->max_mana = ch->raw_mana;
  bonus = mana_bonus[GET_INT(ch)];

  if ((GET_CLASS(ch) == CLASS_ANTI_PAL) ||
     (GET_CLASS(ch) == CLASS_PALADIN))
    bonus /= 2;

  if ((GET_CLASS(ch) == CLASS_WARRIOR) || (GET_CLASS(ch) == CLASS_THIEF) ||
      (GET_CLASS(ch) == CLASS_BARBARIAN) || (GET_CLASS(ch) == CLASS_MONK))
    bonus = 0;

  ch->max_mana += bonus;

  for (i=0; i<MAX_WEAR; i++) 
  {
    if (ch->equipment[i])
      for (j=0; j<ch->equipment[i]->num_affects; j++) 
      {
        if (ch->equipment[i]->affected[j].location == APPLY_MANA)
          affect_modify(ch, ch->equipment[i]->affected[j].location,
                           ch->equipment[i]->affected[j].modifier,
                           0, TRUE);
      }
  }
}

/* Gain maximum in various */
void advance_level(CHAR_DATA *ch, int is_conversion)
{
    int add_hp = 0;
    int add_mana = 0;
    int add_moves = 0;
    int add_ki = 0;
    int add_practices;
    int i;
    char buf[MAX_STRING_LENGTH];

    extern struct wis_app_type wis_app[];
    extern struct con_app_type con_app[];

    switch(GET_CLASS(ch)) { 
    case CLASS_MAGIC_USER:
	add_ki	    += (GET_LEVEL(ch) % 2);
	add_hp      += number(2, 6);
	add_mana    += number(2, 5);
	add_moves   += number(1, (GET_CON(ch) / 2));
	break;

    case CLASS_CLERIC:
	add_ki	    += (GET_LEVEL(ch) % 2);
	add_hp      += number(3, 13);
	add_mana    += number(2, 5);
	add_moves   += number(1, (GET_CON(ch) / 2));
	break;

    case CLASS_THIEF:
	add_ki	    += (GET_LEVEL(ch) % 2);
	add_hp      += number(3 , 11);
	add_moves   += number(1, (GET_CON(ch) / 2));
	break;

    case CLASS_WARRIOR:
	add_ki	    += (GET_LEVEL(ch) % 2);
	add_hp      += number(6, 21);
	add_moves   += number(1, (GET_CON(ch) / 2));
	break;

    case CLASS_ANTI_PAL:
	add_ki	    += (GET_LEVEL(ch) % 2);
        add_hp      += number(8, 15);
        add_mana    += number(1, 3);
        add_moves   += number(1, (GET_CON(ch) / 2));
        break;

    case CLASS_PALADIN:
	add_ki	    += (GET_LEVEL(ch) % 2);
        add_hp      += number(8, 15);
        add_mana    += number(1, 3);
        add_moves   += number(1, (GET_CON(ch) / 2));
        break;
   
    case CLASS_BARBARIAN:
	add_ki	     += (GET_LEVEL(ch) % 2);
        add_hp       += number(10,23);
        add_moves    += number(1, (GET_CON(ch) / 2));
        break;

     case CLASS_MONK:
	add_ki	     += 1;
        add_hp       += number(4, 13);
        add_moves    += number(1, (GET_CON(ch) / 2));
        GET_AC(ch)   += -2;
        break;

     case CLASS_RANGER:
	add_ki       += (GET_LEVEL(ch) % 2);
        add_hp       += number(8, 15);
        add_mana     += number(1, 3);
        add_moves    += number(1, (GET_CON(ch) / 2)); 
        break;

     case CLASS_BARD:
       add_ki       += 1;
       add_hp       += number(8, 15);
       add_mana     += 0;
       add_moves    += number(1, (GET_CON(ch) / 2)); 
       break;

     case CLASS_DRUID:
       add_ki       += (GET_LEVEL(ch) % 2);;
       add_hp       += number(3, 8);
       add_mana     += number(2, 5);
       add_moves    += number(1, (GET_CON(ch) / 2)); 
       break;

     default:  log("Unknown class in advance level?", OVERSEER, LOG_BUG);
       return;
    }

    if ((GET_CLASS(ch) == CLASS_MAGIC_USER) ||
        (GET_CLASS(ch) == CLASS_CLERIC) ||
        (GET_CLASS(ch) == CLASS_DRUID))
       add_hp += number(0, (con_app[GET_CON(ch)].hitp/2));
    else add_hp += number(1, con_app[GET_CON(ch)].hitp);

    add_hp			 = MAX( 1, add_hp);
    add_mana			 = MAX( 0, add_mana);
    add_moves			 = MAX( 1, add_moves);
    add_practices		 = wis_app[GET_WIS(ch)].bonus;

    // hp and mana have stat bonuses related to level so have to have their stuff recalculated
    ch->raw_hit			+= add_hp;
    ch->raw_mana		+= add_mana;
    redo_hitpoints(ch);
    redo_mana (ch);

    // move and ki aren't stat related, so we just add directly to the totals
    ch->raw_move		+= add_moves;
    ch->max_move		+= add_moves;
    ch->raw_ki			+= add_ki;
    ch->max_ki			+= add_ki;

    if(!IS_MOB(ch) && !is_conversion)
      ch->pcdata->practices	+= add_practices;

    sprintf( buf,
	"Your gain is: %d/%ld hp, %d/%ld m, %d/%ld mv, %d/%ld prac, %d/%ld ki.\n\r",
	add_hp,        GET_MAX_HIT(ch),
	add_mana,      GET_MAX_MANA(ch),
	add_moves,     GET_MAX_MOVE(ch),
	IS_MOB(ch) ? 0 : add_practices, IS_MOB(ch) ? 0 : ch->pcdata->practices,
	add_ki,        GET_MAX_KI(ch)
	);
    if(!is_conversion)
      send_to_char( buf, ch );

    if(GET_LEVEL(ch) % 2 == 0)
      for(int i = 0; i <= SAVE_TYPE_MAX; i++)
        ch->saves[i]++;

    if (GET_LEVEL(ch) > IMMORTAL)
	for (i = 0; i < 3; i++)
	    ch->conditions[i] = -1;
}   

void gain_exp( CHAR_DATA *ch, int gain )
{
  int x = 0;
  long y;

  if(!IS_NPC(ch) && GET_LEVEL(ch) >= IMMORTAL)  
    return;

  y = exp_table[GET_LEVEL(ch)+1];  

  if(GET_EXP(ch) >= (int)y)
    x = 1; 

  if(GET_EXP(ch) > 2000000000)
  {
    send_to_char("You have hit the 2 billion xp cap.  Convert or meta chode.\r\n", ch);
    return;
  }
  GET_EXP(ch) += gain;
  if( GET_EXP(ch) < 0 )
    GET_EXP(ch) = 0;

  if(IS_NPC(ch))
    return;

  if(!x && GET_EXP(ch) >= (int)y)
     send_to_char("You now have enough experience to level!\n\r", ch);

  return;
}


void gain_exp_regardless( CHAR_DATA *ch, int gain )
{
  GET_EXP(ch) += gain;

  if(GET_EXP(ch) < 0)
    GET_EXP(ch) = 0;

  if(IS_NPC(ch))
    return;

  while(GET_EXP(ch) >= exp_table[GET_LEVEL(ch) + 1]) {
    send_to_char( "You raise a level!!  ", ch );
    GET_LEVEL(ch) += 1;
    advance_level(ch, 0);
  }

  return;
}

void gain_condition(CHAR_DATA *ch,int condition,int value)
{
    bool intoxicated;

    if(GET_COND(ch, condition)==-1) /* No change */
	return;

    intoxicated=(GET_COND(ch, DRUNK) > 0);

    GET_COND(ch, condition)  += value;

    GET_COND(ch,condition) = MAX(0,(int)GET_COND(ch,condition));
    GET_COND(ch,condition) = MIN(24,(int)GET_COND(ch,condition));

    if(GET_COND(ch,condition))
	return;

    switch(condition){
	case FULL :
	{
	    send_to_char("You are hungry.\n\r",ch);
	    return;
	}
	case THIRST :
	{
	    send_to_char("You are thirsty.\n\r",ch);
	    return;
	}
	case DRUNK :
	{
	    if(intoxicated)
		send_to_char("You are now sober.\n\r",ch);
	    return;
	}
	default : break;
    }

    // just for fun
    if(1 == number(1, 2000)) {
      send_to_char("You are horny\r\n", ch);
    }
}

void food_update( void )
{
  struct obj_data * bring_type_to_front(char_data * ch, int item_type);
  int do_eat(struct char_data *ch, char *argument, int cmd);
  int do_drink(struct char_data *ch, char *argument, int cmd);
  int FOUNTAINisPresent (CHAR_DATA *ch);

  CHAR_DATA *i, *next_dude;
  struct obj_data * food = NULL;

  for(i = character_list; i; i = next_dude) 
  {
    next_dude = i->next;

    gain_condition(i,FULL,-1);
    if(!GET_COND(i, FULL)) { // i'm hungry
      if(!IS_MOB(i) && IS_SET(i->pcdata->toggles, PLR_AUTOEAT) && (GET_POS(i) > POSITION_SLEEPING)) {
        if(FOUNTAINisPresent(i)) {
          do_drink(i, "fountain", 9);
        }
        else if((food =  bring_type_to_front(i, ITEM_FOOD)))
          do_eat(i, food->name, 9);
        else send_to_char("You are out of food.\n\r", i);
      }
    }          
    gain_condition(i,DRUNK,-1);
    gain_condition(i,THIRST,-1);
    if(!GET_COND(i, THIRST)) { // i'm thirsty
      if(!IS_MOB(i) && IS_SET(i->pcdata->toggles, PLR_AUTOEAT) && (GET_POS(i) > POSITION_SLEEPING)) {
        if(FOUNTAINisPresent(i)) {
          do_drink(i, "fountain", 9);
        }
        else if((food =  bring_type_to_front(i, ITEM_DRINKCON)))
          do_drink(i, food->name, 9);
        else send_to_char("You are out of drink.\n\r", i);
      }          
    }
  }
}

// Update the HP of mobs and players
//
void point_update( void )
{   

  CHAR_DATA *i, *next_dude;

  /* characters */
  for(i = character_list; i; i = next_dude) 
  {
    next_dude = i->next;

    // only heal linkalive's and mobs
    if(GET_POS(i) > POSITION_DEAD && (IS_NPC(i) || i->desc)) {
      GET_HIT(i)  = MIN(GET_HIT(i)  + hit_gain(i),  hit_limit(i));
      GET_MANA(i) = MIN(GET_MANA(i) + mana_gain(i), mana_limit(i));
      GET_MOVE(i) = MIN(GET_MOVE(i) + move_gain(i), move_limit(i));
      GET_KI(i)   = MIN(GET_KI(i)   + ki_gain(i),   ki_limit(i));
    }

  } /* for */
}

void update_corpses_and_portals(void)
{
  struct obj_data *j, *next_thing;
  struct obj_data *jj, *next_thing2;

  void extract_obj(struct obj_data *obj); /* handler.c */

  /* objects */
  for(j = object_list; j ; j = next_thing)
  {
    next_thing = j->next; /* Next in object list */

    /* Type 1 is a permanent game portal, and type 3 is a look_only
    |  object.  Type 0 is the spell portal and type 2 is a game_portal
    */
    if((GET_ITEM_TYPE(j) == ITEM_PORTAL) && (j->obj_flags.value[1] == 0
        || j->obj_flags.value[1] == 2)) 
    {
      if(j->obj_flags.timer > 0)
        (j->obj_flags.timer)--;
        if(!(j->obj_flags.timer)) 
        {
          if((j->in_room != NOWHERE) && (world[j->in_room].people)) {
            act("$p shimmers brightly and then fades away.", world[j->in_room].people, j, 0, TO_ROOM, INVIS_NULL);
            act("$p shimmers brightly and then fades away.", world[j->in_room].people, j, 0, TO_CHAR, INVIS_NULL);
          }
          extract_obj(j);
          continue;
        }
    }

    /* If this is a corpse */
    else if((GET_ITEM_TYPE(j) == ITEM_CONTAINER) &&
            (j->obj_flags.value[3]))
    {
            // TODO ^^^ - makes value[3] for containers a bitvector instead of a boolean

      /* timer count down */
      if (j->obj_flags.timer > 0) j->obj_flags.timer--;

      if (!j->obj_flags.timer) 
      {
        if (j->carried_by)
          act("$p decays in your hands.", j->carried_by, j, 0, TO_CHAR, 0);
        else if ((j->in_room != NOWHERE) && (world[j->in_room].people))
        {
          act("A quivering horde of maggots consumes $p.", world[j->in_room].people, j, 0, TO_ROOM, INVIS_NULL);
          act("A quivering horde of maggots consumes $p.", world[j->in_room].people, j, 0, TO_CHAR, 0);
        }

        for(jj = j->contains; jj; jj = next_thing2) 
        {
          next_thing2 = jj->next_content; /* Next in inventory */

          if (j->in_obj)
            move_obj(jj, j->in_obj);
          else if (j->carried_by)
            move_obj(jj, j->carried_by);
          else if (j->in_room != NOWHERE)
          {
            // no trade objects disappear when the corpse decays
            if(IS_SET(jj->obj_flags.more_flags, ITEM_NO_TRADE))
              extract_obj(jj);
            else move_obj(jj, j->in_room);
          }
          else {
            log("BIIIG problem in limits.c!", OVERSEER, LOG_BUG);
            return;
          }
        }

        extract_obj(j);
      }
    }
  }
  /* Now process the portals */
  process_portals();
}
