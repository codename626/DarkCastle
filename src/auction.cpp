/*

Brand new auction file!

*/

extern "C"
{
#include <ctype.h>
#include <string.h>
}
#include <obj.h>
#include <spells.h>
#include <player.h> 
#include <terminal.h> 
#include <levels.h> 
#include <character.h> 
#include <room.h>
#include <utility.h> 
#include <assert.h>
#include <db.h>
#include <vault.h>
#include <returnvals.h>
#include <interp.h>
#include <string>
#include <map>
#include <queue>

using namespace std;

#define auction_duration 1209600

struct obj_data * search_char_for_item(char_data * ch, int16 item_number, bool wearingonly = FALSE);
extern struct index_data *obj_index;
extern struct index_data *mob_index;
extern CWorld world;
//extern struct descriptor_data *descriptor_list;

enum ListOptions
{
  LIST_ALL = 0,
  LIST_MINE,
  LIST_PRIVATE,
  LIST_BY_NAME,
  LIST_BY_LEVEL,
  LIST_BY_SLOT
};

enum AuctionStates
{
  AUC_FOR_SALE = 0,
  AUC_EXPIRED,
  AUC_SOLD,
  AUC_DELETED
};

/*
TICKET STRUCT
*/
struct AuctionTicket
{
  int vitem;
  string item_name;
  unsigned int price;
  string seller;
  string buyer;
  AuctionStates state;
  unsigned int end_time;
};

/*
CLASS DEFINE
*/
class AuctionHouse
{
public:
  AuctionHouse(string in_file);
  AuctionHouse();
  ~AuctionHouse();
  void CollectTickets(CHAR_DATA *ch, unsigned int ticket = 0);
  void CancelAll(CHAR_DATA *ch);
  void AddItem(CHAR_DATA *ch, OBJ_DATA *obj, unsigned int price, string buyer);
  void RemoveTicket(CHAR_DATA *ch, unsigned int ticket);
  void BuyItem(CHAR_DATA *ch, unsigned int ticket);
  void ListItems(CHAR_DATA *ch, ListOptions options, string name, unsigned int to, unsigned int from);
  void CheckExpire();
  void Identify(CHAR_DATA *ch, unsigned int ticket);
  void AddRoom(CHAR_DATA *ch, int room);
  void RemoveRoom(CHAR_DATA *ch, int room);
  void ListRooms(CHAR_DATA *ch);
  void HandleRename(CHAR_DATA *ch, string old_name, string new_name);
  void HandleDelete(string name);
  bool IsAuctionHouse(int room);
  void Save();
  void Load();  
private:
  bool CanSellMore(CHAR_DATA *ch);
  bool IsOkToSell(OBJ_DATA *obj);
  bool IsWearable(CHAR_DATA *ch, int vnum);
  bool IsNoTrade(int vnum);
  bool IsExist(string name, int vnum);
  bool IsName(string name, int vnum);
  bool IsSlot(string slot, int vnum);
  bool IsLevel(unsigned int to, unsigned int from, int vnum);
  map<int, int> auction_rooms;
  unsigned int cur_index;
  string file_name;
  map<unsigned int, AuctionTicket> Items_For_Sale;
};


/*
Handle deletes/zaps
*/
void AuctionHouse::HandleDelete(string name)
{
  map<unsigned int, AuctionTicket>::iterator Item_it;
  queue<unsigned int> tickets_to_delete;
  for(Item_it = Items_For_Sale.begin(); Item_it != Items_For_Sale.end(); Item_it++)
  {
    if(!Item_it->second.seller.compare(name))
    {
      Item_it->second.state = AUC_DELETED;
      tickets_to_delete.push(Item_it->first);    
    }
  }
  
  char buf[MAX_STRING_LENGTH];
  sprintf(buf, "%u auctions belonging to %s have been deleted.", 
                tickets_to_delete.size(), name.c_str());
  log(buf, ANGEL, LOG_GOD);

  while(!tickets_to_delete.empty())
  {  
      Items_For_Sale.erase(tickets_to_delete.front());
      tickets_to_delete.pop();
  }
  Save();
  return;
}

/*
Handle Renames
*/
void AuctionHouse::HandleRename(CHAR_DATA *ch, string old_name, string new_name)
{
  map<unsigned int, AuctionTicket>::iterator Item_it;
  unsigned int i = 0;

  for(Item_it = Items_For_Sale.begin(); Item_it != Items_For_Sale.end(); Item_it++)
  {
    if(!Item_it->second.seller.compare(old_name))
    {
      i++;
      Item_it->second.seller = new_name;
    }
  }
  char buf[MAX_STRING_LENGTH];
  sprintf(buf, "%u auctions have been converted from %s to %s.", i, old_name.c_str(), new_name.c_str());
  log(buf, GET_LEVEL(ch), LOG_GOD);
  Save();
  return;
}

/*
Is the player selling the max # of items already?
*/
bool AuctionHouse::CanSellMore(CHAR_DATA *ch)
{
  struct vault_data *vault;
  int max_items;
  int items_sold = 0;

  if (!(vault = has_vault(GET_NAME(ch)))) 
    return false;

  max_items = vault->size / 100;

  map<unsigned int, AuctionTicket>::iterator Item_it;
  
  for(Item_it = Items_For_Sale.begin(); Item_it != Items_For_Sale.end(); Item_it++)
  {
    if(!Item_it->second.seller.compare(GET_NAME(ch)))
      items_sold++;
  }

  return (items_sold < max_items);
}

/*
Is item type ok to sell?
*/
bool AuctionHouse::IsOkToSell(OBJ_DATA *obj)
{
  switch(obj->obj_flags.type_flag)
  {
    case ITEM_LIGHT:
    case ITEM_SCROLL:
    case ITEM_WAND:
    case ITEM_STAFF:
    case ITEM_WEAPON:
    case ITEM_FIREWEAPON:
    case ITEM_MISSILE:
    case ITEM_TREASURE:
    case ITEM_ARMOR:
    case ITEM_POTION:
    case ITEM_CONTAINER:
    case ITEM_DRINKCON:
    case ITEM_INSTRUMENT:
    case ITEM_LOCKPICK:
    case ITEM_BOAT:
    return true;
    break;
    default:
    return false;
    break;
  }

  return false;
}


/*
Identify an item.
*/
void AuctionHouse::Identify(CHAR_DATA *ch, unsigned int ticket)
{
  int spell_identify(ubyte level, CHAR_DATA *ch, CHAR_DATA *victim, struct obj_data *obj, int skill);
  map<unsigned int, AuctionTicket>::iterator Item_it;

  if((Item_it = Items_For_Sale.find(ticket)) == Items_For_Sale.end())
  {
    csendf(ch, "Ticket number %d doesn't seem to exist.\n\r", ticket);
    return;
  }

  if(!Item_it->second.buyer.empty() && Item_it->second.buyer.compare(GET_NAME(ch)))
  {
    csendf(ch, "Ticket number %d is private.\n\r", ticket);
    return;
  }

  if(GET_GOLD(ch) < 6000)
  {
    send_to_char("The broker charges 6000 gold to identify items.\n\r", ch);
    return;
  }

  int nr = real_object(Item_it->second.vitem);
  if(nr < 0)
  {
    csendf(ch, "There is a problem with ticket %d. Please tell an imm.\n\r", ticket);
    return;
  }

  OBJ_DATA *obj = (struct obj_data *)(obj_index[nr].item);

  GET_GOLD(ch) -= 6000;
  send_to_char("You pay the broker 6000 gold to identify the item.\n\r", ch);

  spell_identify(60, ch, NULL, obj, 100);

  return;
 
}

/*
Is item of type slot?
*/
bool AuctionHouse::IsSlot(string slot, int vnum)
{
  int keyword;
  char buf[MAX_STRING_LENGTH];
  strncpy(buf, slot.c_str(), MAX_STRING_LENGTH);

  static char *keywords[] = 
  {
        "finger",//0
        "neck",//1
        "body",//2
        "head",//3
        "legs",//4
        "feet",//5
        "hands",//6
        "arms",//7
        "about",//8
        "waist",//9
        "wrist",//10
        "face",//11
        "wield",//12
        "shield",//13
        "hold",//14
        "ear",//15
        "light",//16
        "\n"
  };

  keyword = search_block(buf, keywords, FALSE);
  if (keyword == -1) 
    return false;
  
 //   char out_buf[MAX_STRING_LENGTH];
  //  sprintf(out_buf, "%s is an unknown body location.\n\r", buf);
   // send_to_char(out_buf, ch);
 
  int nr = real_object(vnum);

  if(nr < 0)
    return true;
 
  OBJ_DATA *obj = (struct obj_data *)(obj_index[nr].item);
  switch(keyword)
  {
    case 0:
    return CAN_WEAR(obj, ITEM_WEAR_FINGER);
    break;
    case 1:
    return CAN_WEAR(obj, ITEM_WEAR_NECK);
    break;
    case 2:
    return CAN_WEAR(obj, ITEM_WEAR_BODY);
    break;
    case 3:
    return CAN_WEAR(obj, ITEM_WEAR_HEAD);
    break;
    case 4:
    return CAN_WEAR(obj, ITEM_WEAR_LEGS);
    break;
    case 5:
    return CAN_WEAR(obj, ITEM_WEAR_FEET);
    break;
    case 6:
    return CAN_WEAR(obj, ITEM_WEAR_HANDS);
    break;
    case 7:
    return CAN_WEAR(obj, ITEM_WEAR_ARMS);
    break;
    case 8:
    return CAN_WEAR(obj, ITEM_WEAR_ABOUT);
    break;
    case 9:
    return CAN_WEAR(obj, ITEM_WEAR_WAISTE);
    break;
    case 10:
    return CAN_WEAR(obj, ITEM_WEAR_WRIST);
    break;
    case 11:
    return CAN_WEAR(obj, ITEM_WEAR_FACE);
    break;
    case 12:
    return CAN_WEAR(obj, ITEM_WIELD);
    break;
    case 13:
    return CAN_WEAR(obj, ITEM_WEAR_SHIELD);
    break;
    case 14:
    return CAN_WEAR(obj, ITEM_HOLD);
    break;
    case 15:
    return CAN_WEAR(obj, ITEM_WEAR_EAR);
    break;
    case 16:
    return CAN_WEAR(obj, ITEM_LIGHT);
    break;
    default:
    return false;
    break;
  }
  return false;;
}

/*
Is the item wearable by the player?
*/
bool AuctionHouse::IsWearable(CHAR_DATA *ch, int vnum)
{
  int class_restricted(struct char_data *ch, struct obj_data *obj);
  int size_restricted(struct char_data *ch, struct obj_data *obj);
  int nr = real_object(vnum);

  if(nr < 0)
    return true;
 
  OBJ_DATA *obj = (struct obj_data *)(obj_index[nr].item);
  return !(class_restricted(ch, obj) || size_restricted(ch, obj) || (obj->obj_flags.eq_level > GET_LEVEL(ch)));
    
}



/*
Is the player already selling the unique item?
*/
bool AuctionHouse::IsExist(string name, int vnum)
{
  map<unsigned int, AuctionTicket>::iterator Item_it;
  for(Item_it = Items_For_Sale.begin(); Item_it != Items_For_Sale.end(); Item_it++)
  {
    if(Item_it->second.vitem == vnum)
      return true;
  }  
  return false;
}

/*
Is the item no_trade?
*/
bool AuctionHouse::IsNoTrade(int vnum)
{
  int nr = real_object(vnum);
  if(nr < 0)
    return false;
  return IS_SET(((struct obj_data *)(obj_index[nr].item))->obj_flags.more_flags, ITEM_NO_TRADE);;
  
}


/*
SEARCY BY LEVEL
*/
bool AuctionHouse::IsLevel(unsigned int to, unsigned int from, int vnum)
{
  int nr;
  if(to > from)
    swap(to, from);
  unsigned int eq_level;
  if(from == 0) from = 999;

  if((nr = real_object(vnum)) < 0)
    return false;

  
  eq_level = ((struct obj_data *)(obj_index[nr].item))->obj_flags.eq_level;

  return (eq_level >= to && eq_level <= from);
}


/*
SEARCH BY NAME
*/
bool AuctionHouse::IsName(string name, int vnum)
{
  int nr;

  if((nr = real_object(vnum)) < 0)
    return false;

  return isname(name.c_str(), ((struct obj_data *)(obj_index[nr].item))->name);
}

/*
IS AUCTION HOUSE?
*/
bool AuctionHouse::IsAuctionHouse(int room)
{
  if(auction_rooms.end() == auction_rooms.find(room))
    return false;
  else
    return true;
}

/*
LIST ROOMS
*/
void AuctionHouse::ListRooms(CHAR_DATA *ch)
{
  map<int, int>::iterator room_it;
  send_to_char("Auction Rooms:", ch);

  for(room_it = auction_rooms.begin(); room_it != auction_rooms.end(); room_it++)
    csendf(ch, " %d", room_it->first); 
  
  send_to_char("\n", ch);
  return;
}

/*
ADD ROOM
*/
void AuctionHouse::AddRoom(CHAR_DATA *ch, int room)
{
  if(auction_rooms.end() == auction_rooms.find(room))
  {
    auction_rooms[room] = 1;
    csendf(ch, "Done. Room %d added to auction houses.\n\r", room);
    logf(GET_LEVEL(ch), LOG_GOD, "%s just added room %d to auction houses.", GET_NAME(ch), room);
    Save();
    return;
  }
  else
    csendf(ch, "Room %d is already an auction house.\n\r", room);
  return;
}

/*
REMOVE ROOM
*/
void AuctionHouse::RemoveRoom(CHAR_DATA *ch, int room)
{
  if(1 == auction_rooms.erase(room))
  {
    csendf(ch, "Done. Room %d has been removed from auction houses.\n\r", room);
    logf(GET_LEVEL(ch), LOG_GOD, "%s just removed room %d from auction houses.", GET_NAME(ch), room);
    Save();
    return;
  }
  else
   csendf(ch, "Room %d doesn't appear to be an auction house.\n\r", room);
  return;
}

/*
LOAD
*/
void AuctionHouse::Load()
{
  FILE * the_file;
  unsigned int num_rooms, num_items, ticket, i, state;
  int room;
  char *nl;
  char buf[MAX_STRING_LENGTH];
  AuctionTicket InTicket;
  the_file = dc_fopen(file_name.c_str(), "r");

  if(!the_file) 
  {
      char buf[MAX_STRING_LENGTH];
      sprintf(buf, "Unable to open the save file \"%s\" for Auction files!!", file_name.c_str());
      log(buf, 0, LOG_MISC);
      return;
  }

  fscanf(the_file, "%u\n", &num_rooms);

  for(i = 0; i < num_rooms; i++)
  {
    fscanf(the_file, "%d\n", &room);
    auction_rooms[room] = 1;
  }

  fscanf(the_file, "%u\n", &num_items);
  for(i = 0; i < num_items; i++)
  {
    fscanf(the_file, "%u\n", &ticket);
    fscanf(the_file, "%d\n", &InTicket.vitem);
    fgets(buf, MAX_STRING_LENGTH, the_file);
    nl = strrchr(buf, '\n');
    if(nl) *nl = '\0'; //fgets grabs newline too, removing it here
    InTicket.item_name = buf;
    fgets(buf, MAX_STRING_LENGTH, the_file);
    nl = strrchr(buf, '\n');
    if(nl) *nl = '\0'; //fgets grabs newline too, removing it here
    InTicket.seller = buf;
    fgets(buf, MAX_STRING_LENGTH, the_file);
    nl = strrchr(buf, '\n');
    if(nl) *nl = '\0'; //fgets grabs newline too, removing it here
    InTicket.buyer = buf; 
    fscanf(the_file, "%u\n", &state);
    InTicket.state = (AuctionStates)state;
    fscanf(the_file, "%u\n", &InTicket.end_time);
    fscanf(the_file, "%u\n", &InTicket.price);
    Items_For_Sale[ticket] = InTicket;
  }
  return;
}

/*
SAVE
*/
void AuctionHouse::Save()
{
  FILE * the_file;
  map<unsigned int, AuctionTicket>::iterator Item_it;
  map<int, int>::iterator room_it;
  extern short bport;

  if(bport)
  {
    log("Unable to save auction files because this is the testport!", ANGEL, LOG_MISC);
    return;
  }

  the_file = dc_fopen(file_name.c_str(), "w");

  if(!the_file) 
  {
      char buf[MAX_STRING_LENGTH];
      sprintf(buf, "Unable to open/create the save file \"%s\" for Auction files!!", file_name.c_str());
      log(buf, ANGEL, LOG_BUG);
      return;
  }

  fprintf(the_file, "%u\n", auction_rooms.size());
  for(room_it = auction_rooms.begin(); room_it != auction_rooms.end(); room_it++)
  {
    fprintf(the_file, "%d\n", room_it->first);
  }

  fprintf(the_file, "%u\n", Items_For_Sale.size());
  for(Item_it = Items_For_Sale.begin(); Item_it != Items_For_Sale.end(); Item_it++)
  {
    fprintf(the_file, "%u\n", Item_it->first);
    fprintf(the_file, "%d\n", Item_it->second.vitem);
    fprintf(the_file, "%s\n", (char*)Item_it->second.item_name.c_str());
    fprintf(the_file, "%s\n", (char*)Item_it->second.seller.c_str());
    fprintf(the_file, "%s\n", (char*)Item_it->second.buyer.c_str()); 
    fprintf(the_file, "%u\n", Item_it->second.state);
    fprintf(the_file, "%u\n", Item_it->second.end_time);
    fprintf(the_file, "%u\n", Item_it->second.price);
  }

  dc_fclose(the_file);
  return;
} 

/*
Destructor
*/
AuctionHouse::~AuctionHouse()
{}

/*
Empty constructor
*/
AuctionHouse::AuctionHouse()
{
  /*
  In the current implementation this must NEVER be called
  */
  assert(0);
}

/*
Constructor with file name
*/
AuctionHouse::AuctionHouse(string in_file)
{
  cur_index = 0;
  file_name = in_file;
}


/*
CANCEL ALL
*/
void AuctionHouse::CancelAll(CHAR_DATA *ch)
{
  map<unsigned int, AuctionTicket>::iterator Item_it;
  queue<unsigned int> tickets_to_cancel;
  for(Item_it = Items_For_Sale.begin(); Item_it != Items_For_Sale.end(); Item_it++)
  {
    if(!Item_it->second.seller.compare(GET_NAME(ch)))
      tickets_to_cancel.push(Item_it->first);    
  }
  if(tickets_to_cancel.empty())
  {
    send_to_char("You have no tickets to cancel!\n\r", ch);
    return;
  }
  while(!tickets_to_cancel.empty())
  {  
      RemoveTicket(ch, tickets_to_cancel.front());
      tickets_to_cancel.pop();
  }
  return;

}

/*
COLLECT EXPIRED
*/
void AuctionHouse::CollectTickets(CHAR_DATA *ch, unsigned int ticket)
{
  map<unsigned int, AuctionTicket>::iterator Item_it;
  if(ticket > 0)
  {
     Item_it = Items_For_Sale.find(ticket);
     if(Item_it != Items_For_Sale.end())
     {
       if(Item_it->second.seller.compare(GET_NAME(ch)))
       {
         csendf(ch, "Ticket %d doesn't seem to belong to you.\n\r", ticket);
         return;
       }
       if(Item_it->second.state != AUC_EXPIRED && Item_it->second.state != AUC_SOLD)
       {
         csendf(ch, "Ticket %d isn't collectible!.\n\r", ticket);
         return;
       }
     }
     RemoveTicket(ch, ticket);
     return;
  }

  queue<unsigned int> tickets_to_remove;
  for(Item_it = Items_For_Sale.begin(); Item_it != Items_For_Sale.end(); Item_it++)
  {
    if((Item_it->second.state == AUC_EXPIRED || Item_it->second.state == AUC_SOLD)
       && !Item_it->second.seller.compare(GET_NAME(ch)))
      tickets_to_remove.push(Item_it->first);    
  }
  if(tickets_to_remove.empty())
  {
    send_to_char("You have nothing to collect!\n\r", ch);
    return;
  }
  while(!tickets_to_remove.empty())
  {  
      RemoveTicket(ch, tickets_to_remove.front());
      tickets_to_remove.pop();
  }
  return;
}


/*
CHECK EXPIRE
*/
void AuctionHouse::CheckExpire()
{
  unsigned int cur_time = time(0);
  CHAR_DATA *ch;
  bool something_expired = false;
  map<unsigned int, AuctionTicket>::iterator Item_it;

  for(Item_it = Items_For_Sale.begin(); Item_it != Items_For_Sale.end(); Item_it++)
  {
    if(Item_it->second.state == AUC_FOR_SALE && cur_time >= Item_it->second.end_time)
    {
      if((ch = get_active_pc(Item_it->second.seller.c_str()))) 
        csendf(ch, "Your auction of %s has expired.\n\r", Item_it->second.item_name.c_str());
      Item_it->second.state = AUC_EXPIRED;
      something_expired = true;
    }
  }

  if(something_expired)
    Save();
}


/*
BUY ITEM
*/
void AuctionHouse::BuyItem(CHAR_DATA *ch, unsigned int ticket)
{ 
  map<unsigned int, AuctionTicket>::iterator Item_it;
  OBJ_DATA *obj;
  CHAR_DATA *vict;

  Item_it = Items_For_Sale.find(ticket);
  if(Item_it == Items_For_Sale.end())
  {
    csendf(ch, "Ticket number %d doesn't seem to exist.\n\r", ticket);
    return;
  }

  if(!Item_it->second.buyer.empty() && Item_it->second.buyer.compare(GET_NAME(ch)))
  {
    csendf(ch, "Ticket number %d is private.\n\r", ticket);
    return;
  }

  if(!Item_it->second.seller.compare(GET_NAME(ch)))
  {
    send_to_char("That's your own item you're selling, dumbass!\n\r", ch);
    return;
  }
  
  if(GET_GOLD(ch) < Item_it->second.price)
  {
    csendf(ch, "Ticket number %d costs %d coins, you can't afford that!\n\r", 
                ticket, Item_it->second.price);
    return;
  }
  
  int rnum = real_object(Item_it->second.vitem);

  if(rnum < 0)
  {
    char buf[MAX_STRING_LENGTH];
    sprintf(buf, "Major screw up in auction(buy)! Item %s[VNum %d] belonging to %s could not be created!", 
                    Item_it->second.item_name.c_str(), Item_it->second.vitem, Item_it->second.seller.c_str());
    log(buf, IMMORTAL, LOG_BUG);
    return;
  }  
  obj = clone_object(rnum);

  if(!obj)
  {
    char buf[MAX_STRING_LENGTH];
    sprintf(buf, "Major screw up in auction(buy)! Item %s[RNum %d] belonging to %s could not be created!", 
                    Item_it->second.item_name.c_str(), rnum, Item_it->second.seller.c_str());
    log(buf, IMMORTAL, LOG_BUG);
    return;
  }

  if (IS_SET(obj->obj_flags.more_flags, ITEM_UNIQUE) && search_char_for_item(ch, obj->item_number)) 
  { 
    send_to_char("Why would you want another one of those?\r\n", ch);
    return;
  }
 
  if (IS_SET(obj->obj_flags.more_flags, ITEM_NO_TRADE))
  {
    OBJ_DATA *no_trade_obj;
    int nr = real_object(27909);

    no_trade_obj = search_char_for_item(ch, nr);

    if(!no_trade_obj) 
    { //27909 == wingding right now (notrade transfer token)
      if(nr > 0)
        csendf(ch, "You need to have \"%s\" to buy a NO_TRADE item.\r\n", ((struct obj_data *)(obj_index[nr].item))->short_description);
      return;
    }
    else
    {
      csendf(ch, "You give your %s to the broker.\n\r", no_trade_obj->short_description);
      extract_obj(no_trade_obj);
    }
  } 
   
  GET_GOLD(ch) -= Item_it->second.price;

  if((vict = get_active_pc(Item_it->second.seller.c_str()))) 
      csendf(vict, "%s just purchased your ticket of  %s for %u coins.\n\r", GET_NAME(ch), Item_it->second.item_name.c_str(), Item_it->second.price);

  csendf(ch, "You have purchased %s for %u coins.\n\r", obj->short_description, Item_it->second.price);

  CHAR_DATA *tmp;
  for(tmp = world[ch->in_room].people; tmp; tmp = tmp->next_in_room)
    if(vict != ch) //vict can be null
      csendf(tmp, "%s just purchased %s's %s\n\r", GET_NAME(ch), Item_it->second.seller.c_str(), obj->short_description);

  Item_it->second.state = AUC_SOLD;
  Save(); 
  char log_buf[MAX_STRING_LENGTH];
  sprintf(log_buf, "VEND: %s bought %s's %s for %u coins.\n\r", 
              GET_NAME(ch), Item_it->second.seller.c_str(), Item_it->second.item_name.c_str(),Item_it->second.price);
  log(log_buf, IMP, LOG_OBJECTS);
  obj_to_char(obj, ch);
  do_save(ch, "", 9);
}

/*
CANCEL ITEM
*/
void AuctionHouse::RemoveTicket(CHAR_DATA *ch, unsigned int ticket)
{
  map<unsigned int, AuctionTicket>::iterator Item_it;
  OBJ_DATA *obj;

  Item_it = Items_For_Sale.find(ticket);
  if(Item_it == Items_For_Sale.end())
  {
    csendf(ch, "Ticket number %d doesn't seem to exist.\n\r", ticket);
    return;
  }
  if(Item_it->second.seller.compare(GET_NAME(ch)))
  {
    csendf(ch, "Ticket number %d doesn't belong to you.\n\r", ticket);
    return;
  }

  switch(Item_it->second.state)
  {
    case AUC_SOLD:
    {
      unsigned int fee = (unsigned int)((double)Item_it->second.price * 0.025);
      if(fee > 500000)
        fee = 500000;
      csendf(ch, "The Broker hands you %u gold coins from your sale of %s.\n\r"
               "He pockets %u gold as a broker's fee.\n\r", 
          (Item_it->second.price - fee), Item_it->second.item_name.c_str(), fee);
      GET_GOLD(ch) += (Item_it->second.price - fee);
      char log_buf[MAX_STRING_LENGTH];
      sprintf(log_buf, "VEND: %s just collected %u coins from their sale of %s (ticket %u).\n\r", 
                         GET_NAME(ch), Item_it->second.price, Item_it->second.item_name.c_str(), ticket);
      log(log_buf, IMP, LOG_OBJECTS);
    }
    break;
    case AUC_EXPIRED: //intentional fallthrough
    case AUC_FOR_SALE:
    {
      int rnum = real_object(Item_it->second.vitem);

      if(rnum < 0)
      {
        char buf[MAX_STRING_LENGTH];
        sprintf(buf, "Major screw up in auction(cancel)! Item %s[VNum %d] belonging to %s could not be created!", 
                    Item_it->second.item_name.c_str(), Item_it->second.vitem, Item_it->second.seller.c_str());
        log(buf, IMMORTAL, LOG_BUG);
        return;
      }

      obj = clone_object(rnum);
   
      if(!obj)
      {
        char buf[MAX_STRING_LENGTH];
        sprintf(buf, "Major screw up in auction(cancel)! Item %s[RNum %d] belonging to %s could not be created!", 
                    Item_it->second.item_name.c_str(), rnum, Item_it->second.seller.c_str());
        log(buf, IMMORTAL, LOG_BUG);
        return;
      }

      csendf(ch, "The Consignment Broker retrieves %s and returns it to you.\n\r", obj->short_description);
      char log_buf[MAX_STRING_LENGTH];
      sprintf(log_buf, "VEND: %s cancelled or collected ticket # %u (%s) that was for sale for %u coins.\n\r", 
                       GET_NAME(ch), ticket, Item_it->second.item_name.c_str(), Item_it->second.price);
      log(log_buf, IMP, LOG_OBJECTS);
      obj_to_char(obj, ch); 
     }
     break;
     case AUC_DELETED:
     {
       char buf[MAX_STRING_LENGTH];
       sprintf(buf, "%s just tried to cheat and collect ticket %u which didn't get erased properly!", GET_NAME(ch), ticket);
       log(buf, IMMORTAL, LOG_BUG);
       Items_For_Sale.erase(ticket);
       return;
     }
     break;
     default: 
       log("Default case reached in Removeticket, contact a coder!", IMMORTAL, LOG_BUG);
     break;
   }

  Item_it->second.state = AUC_DELETED; //just a safety precaution
  do_save(ch, "", 9);

  if(1 != Items_For_Sale.erase(ticket))
  {
    char buf[MAX_STRING_LENGTH];
    sprintf(buf, "Major screw up in auction(cancel)! Ticket %d belonging to %s could not be removed!", 
                    ticket, Item_it->second.seller.c_str());
    log(buf, IMMORTAL, LOG_BUG);
    return;
  }

  Save();
  return;
}

/*
LIST ITEMS
*/
void AuctionHouse::ListItems(CHAR_DATA *ch, ListOptions options, string name, unsigned int to, unsigned int from)
{
  map<unsigned int, AuctionTicket>::iterator Item_it;
  int i;
  string output_buf;
  string state_output;
  char buf[MAX_STRING_LENGTH];
  
  send_to_char("Ticket-Seller-------Price------Status--T--Item---------------------------\n\r", ch);
  for(i = 0, Item_it = Items_For_Sale.begin(); (i < 50) && (Item_it != Items_For_Sale.end()); Item_it++)
  {
    if(
       (options == LIST_MINE && !Item_it->second.seller.compare(GET_NAME(ch)))
       || (options == LIST_ALL 
           && (Item_it->second.buyer.empty() || !Item_it->second.buyer.compare(GET_NAME(ch))))
       || (options == LIST_PRIVATE && !Item_it->second.buyer.compare(GET_NAME(ch)))
       || (options == LIST_BY_NAME && IsName(name, Item_it->second.vitem))
       || (options == LIST_BY_LEVEL && IsLevel(to, from, Item_it->second.vitem))
       || (options == LIST_BY_SLOT && IsSlot(name, Item_it->second.vitem))
      )
    {
      if((Item_it->second.state == AUC_EXPIRED || Item_it->second.state == AUC_SOLD) 
          && options != LIST_MINE)
        continue;
      i++;
      switch(Item_it->second.state)
      {
        case AUC_SOLD:
          state_output = "$0$BSOLD$R   ";
        break;
        case AUC_FOR_SALE:
          state_output = "$2ACTIVE$R ";
        break;
        case AUC_EXPIRED:
          state_output = "$4EXPIRED$R";
        break;
        default:
          state_output = "$7ERROR$R  ";
        break; 
      }
      sprintf(buf, "\n\r%05d) $7$B%-12s$R $5%-10d$R %s %s %s%-30s\n\r", 
               Item_it->first, Item_it->second.seller.c_str(), Item_it->second.price,
               state_output.c_str(), IsNoTrade(Item_it->second.vitem) ? "$4N$R" : " ",
               IsWearable(ch, Item_it->second.vitem) ? " " : "$4*$R", Item_it->second.item_name.c_str());
      output_buf += buf;
    }
  }

  if(i == 0)
  {
    if(options == LIST_BY_SLOT)
      csendf(ch, "There are no %s items currently posted.\n\r", name.c_str());
    else
      send_to_char("There is nothing for sale!\n\r", ch);
  }
 
  if(i >= 50)
   send_to_char("Maximum number of results reached.\n\r", ch);


  page_string(ch->desc, output_buf.c_str(), 1);
  if(options == LIST_MINE)
  {
    struct vault_data *vault;
    if ((vault = has_vault(GET_NAME(ch)))) 
    {
       int max_items = vault->size / 100;
       csendf(ch, "You using %d of your %d available tickets.\n\r", i, max_items);
    }
  }
  int nr = real_object(27909);
  if(nr >= 0)
    csendf(ch, "\n\r'$4N$R' indicates an item is NO_TRADE and requires %s to purchase.\n\r",
                ((struct obj_data *)(obj_index[nr].item))->short_description);
  send_to_char("'$4*$R' indicates you are unable to use this item.\n\r", ch);
  return;
}

/*
ADD ITEM
*/
void AuctionHouse::AddItem(CHAR_DATA *ch, OBJ_DATA *obj, unsigned int price, string buyer)
{
  char buf[20];
  strncpy(buf, buyer.c_str(), 19);
  buf[19] = '\0';
  unsigned int fee;
  bool advertise = false;

  //taken from linkload, formatting of player name
  char *c;
  c = buf;
  *c = UPPER(*c);
  c++;
  while(*c) { 
    *c = LOWER(*c);
    c++;
  }

  if(!obj)
  {
    send_to_char("You don't seem to have that item.\n\r", ch);
    return;
  }

  if(!IsOkToSell(obj))
  {
    send_to_char("You can't sell that type of item here!\n\r", ch);
    return;
  }

  if(!CanSellMore(ch))
  {
    struct vault_data *vault;
    if ((vault = has_vault(GET_NAME(ch)))) 
    {
       int max_items = vault->size / 100;
       csendf(ch, "You cannot list more than %d items!\n\r", max_items);
    }
    else
      send_to_char("You have to be level 10 and own a vault to sell items!\n\r", ch);
    return;
  }

  if(IS_SET(obj->obj_flags.extra_flags, ITEM_NOSAVE))
  {
    send_to_char("You can't sell that item!\n\r", ch);
    return;
  }
  
  if(IS_SET(obj->obj_flags.extra_flags, ITEM_SPECIAL)) 
  {
    send_to_char("That sure would be a fucking stupid thing to do.\n\r", ch);
    return;
  }

  if(price > 2000000000)
  {
    send_to_char("Price must be between 1000 and 2000000000.\n\r", ch);
    return;
  }
  
  unsigned int full_checker = cur_index;
  while(Items_For_Sale.end() != Items_For_Sale.find(++cur_index))
  {
    if(cur_index > 99999)
      cur_index = 1;
    if(full_checker == cur_index)
    {
      send_to_char("The auctioneer is selling too many items already!\n\r", ch);
      return;
    }
  }

  fee = (unsigned int)((double)price * 0.025); //2.5% fee to list, then 2.5% fee during sale
  if(fee > 500000)
    fee = 500000;
  if(GET_GOLD(ch) < fee)
  {
    csendf(ch, "You don't have enough gold to pay the %d coin auction fee.\n\r", fee);
    return;
  }

  if(!strcmp(buf, "Advertise"))
    advertise = true;

  if(advertise == true && (GET_GOLD(ch) < (200000 + fee)))
  {
    csendf(ch, "You need 200000 gold plus the %u gold fee to advertise an item.\n\r", fee);
    return;
  }

  if (IS_SET(obj->obj_flags.more_flags, ITEM_UNIQUE) && IsExist(GET_NAME(ch), obj_index[obj->item_number].virt))
  {
    csendf(ch, "You're selling one %s already!\n\r", obj->short_description);
    return;
  }

  if(strcmp(obj->short_description, ((struct obj_data *)(obj_index[obj->item_number].item))->short_description))
  {
    send_to_char("The Consignment broker informs you that he does not handle items that have been restrung.\n\r", ch);
    return;
  }

  if(eq_current_damage(obj) > 0)
  {
    send_to_char("The Consignment Broker curtly informs you that all items sold must be in $B$2Excellent Condition$R.\n\r", ch);
    return;
  }

  GET_GOLD(ch) -= fee;
  csendf(ch, "You pay the %d coin tax to auction the item.\n\r", fee);
 
  AuctionTicket NewTicket;
  NewTicket.vitem = obj_index[obj->item_number].virt;
  NewTicket.price = price;
  NewTicket.state = AUC_FOR_SALE;
  NewTicket.end_time = time(0) + auction_duration;
  NewTicket.seller = GET_NAME(ch);
  NewTicket.item_name = obj->short_description;
  if(advertise == false)
    NewTicket.buyer = buf;

  Items_For_Sale[cur_index] = NewTicket;
  Save();
  
  if(buyer.empty())
    csendf(ch, "You are now selling %s for %d coins.\n\r", 
              obj->short_description, price);
  else
  {
    csendf(ch, "You are now selling %s to %s for %d coins.\n\r", 
              obj->short_description, buyer.c_str(), price);
  }
 
  if(advertise == true)
  {
    char_data * find_mob_in_room(struct char_data *ch, int iFriendId);
    char auc_buf[MAX_STRING_LENGTH];
    CHAR_DATA *Broker = find_mob_in_room(ch, 5258);
    if(Broker)
    {
      GET_GOLD(ch) -= 200000;
      send_to_char("You pay the 200000 gold to advertise your item.\n\r", ch);
      snprintf(auc_buf, MAX_STRING_LENGTH, "$7$B%s has just posted $R%s $7$Bfor sale.",
                         GET_NAME(ch), obj->short_description);
      do_auction(Broker, auc_buf, 9); 
    }
    else
      send_to_char("The Consignment Broker couldn't auction. Contact an imm.\n\r", ch);
  }
 
  char log_buf[MAX_STRING_LENGTH];
  sprintf(log_buf, "VEND: %s just listed %s for sale for %u coins.\n\r", 
               GET_NAME(ch), obj->short_description, price);
  log(log_buf, IMP, LOG_OBJECTS);
  extract_obj(obj);
  do_save(ch, "", 9);
  return;

}


/*
This is the actual auction house creation
*/
AuctionHouse TheAuctionHouse("auctionhouse");


/*
AUCTION_EXPIRE
Called externally from heartbeat
*/
void auction_expire()
{
  TheAuctionHouse.CheckExpire();
  return;
}

/*
HANDLE RENAMES
Called externally in wiz_110 (do_rename)
*/
void AuctionHandleDelete(string name)
{
  TheAuctionHouse.HandleDelete(name);
  return;
}

/*
HANDLE RENAMES
Called externally in wiz_110 (do_rename)
*/
void AuctionHandleRenames(CHAR_DATA *ch, string old_name, string new_name)
{
  TheAuctionHouse.HandleRename(ch, old_name, new_name);
  return;
}


/*
LOAD AUCTION TICKETS
called externally from db.cpp during boot
*/
void load_auction_tickets()
{
  TheAuctionHouse.Load();
  return;
}

int do_vend(CHAR_DATA *ch, char *argument, int cmd)
{
  char buf[MAX_STRING_LENGTH];
  OBJ_DATA *obj;
  unsigned int price;



  if(!TheAuctionHouse.IsAuctionHouse(ch->in_room) && GET_LEVEL(ch) < 108)
  {
    send_to_char("You must be in an auction house to do this!\n\r", ch);
    return eFAILURE;
  }

 if(affected_by_spell(ch, FUCK_PTHIEF) || (affected_by_spell(ch, FUCK_GTHIEF))) {
        send_to_char("You're too busy running from the law!\r\n",ch);
        return eFAILURE;
  }

  argument = one_argument(argument, buf);

  if(!*buf)
  {
    send_to_char("Syntax: vend <buy | sell | list | cancel | collect | search | identify>\n\r", ch);
    return eSUCCESS;       

  }


  /*BUY*/
  if(!strcmp(buf, "search"))
  {
    argument = one_argument(argument, buf);
    if(!*buf)
    {
      send_to_char("Search by what?\n\rSyntax: vend search <name | level | slot>\n\r", ch);
      return eSUCCESS;
    }
    if(!strcmp(buf, "name"))
    {
      argument = one_argument(argument, buf);
      if(!*buf)
      {
        send_to_char("What name do you want to search for?\n\rSyntax: vend search name <keyword>\n\r", ch);
        return eSUCCESS;
      }
      TheAuctionHouse.ListItems(ch, LIST_BY_NAME, buf, 0, 0);

      return eSUCCESS;
    }

    if(!strcmp(buf, "slot"))
    {
      argument = one_argument(argument, buf);
      if(!*buf)
      {
        send_to_char("What slot do you want to search for?\n\r"
                     "finger, neck, body, head, legs, feet, hands, arms, shield,\n\r"
                     "about, waist, wrist, wield, hold, throw, light, face, ear\n\r"
                     "\n\rSyntax: vend search slot <keyword>\n\r", ch);
        return eSUCCESS;
      }
      TheAuctionHouse.ListItems(ch, LIST_BY_SLOT, buf, 0, 0);

      return eSUCCESS;
    }
 
 
    if(!strcmp(buf, "level"))
    {
      unsigned int level;
      argument = one_argument(argument, buf);
      if(!*buf)
      {
        send_to_char("What level?\n\rSyntax: vend search level <min_level> [max_level]\n\r", ch);
        return eSUCCESS;
      }
      level = atoi(buf);
      argument = one_argument(argument, buf);
      if(!*buf)
        TheAuctionHouse.ListItems(ch, LIST_BY_LEVEL, "", level, 0);
      else
        TheAuctionHouse.ListItems(ch, LIST_BY_LEVEL, "", level, atoi(buf));
      return eSUCCESS;
    }

    send_to_char("Search by what?\n\rSyntax: vend search <name | level | slot>\n\r", ch);
    return eSUCCESS;
  }  

  /*COLLECT*/
  if(!strcmp(buf, "collect"))
  {
    argument = one_argument(argument, buf);
    if(!*buf)
    {
      send_to_char("Collect what?\n\rSyntax: vend collect <all | ticket#>\n\r", ch);
      return eSUCCESS;
    }
    if(!strcmp(buf, "all"))
    {
      TheAuctionHouse.CollectTickets(ch);
      return eSUCCESS;
    }
    if(atoi(buf) > 0)
    {
      TheAuctionHouse.CollectTickets(ch, atoi(buf));
      return eSUCCESS;
    }
    
    send_to_char("Syntax: vend collect <all | ticket#>\n\r", ch);
    return eSUCCESS;
  }
 
  /*BUY*/
  if(!strcmp(buf, "buy"))
  {
    argument = one_argument(argument, buf);
    if(!*buf)
    {
      send_to_char("Buy what?\n\rSyntax: vend buy <ticket #>\n\r", ch);
      return eSUCCESS;
    }
    TheAuctionHouse.BuyItem(ch, atoi(buf));
    return eSUCCESS;
  }

  /*CANCEL*/
  if(!strcmp(buf, "cancel"))
  {
    argument = one_argument(argument, buf);
    if(!*buf)
    {
      send_to_char("Cancel what?\n\rSyntax: vend cancel <all | ticket#>\n\r", ch);
      return eSUCCESS;
    }
    if(!strcmp(buf, "all")) //stupid cancel all didn't fit my design, but the boss wanted it
    {
      TheAuctionHouse.CancelAll(ch);
      return eSUCCESS;
    }
    TheAuctionHouse.RemoveTicket(ch, atoi(buf));
    return eSUCCESS;
  }
 
  /*LIST*/
  if(!strcmp(buf, "list"))
  {
    argument = one_argument(argument, buf);
    if(!*buf)
    {
      send_to_char("List what?\n\rSyntax: vend list <all | mine | private>\n\r", ch);
      return eSUCCESS;
    }

    if(!strcmp(buf, "all"))
    {
      TheAuctionHouse.ListItems(ch, LIST_ALL, "", 0, 0);
    }
    else if (!strcmp(buf, "mine"))
    {
      TheAuctionHouse.ListItems(ch, LIST_MINE, "", 0, 0);
    }
    else if (!strcmp(buf, "private"))
    {
      TheAuctionHouse.ListItems(ch, LIST_PRIVATE, "", 0, 0);
    }
    else
    {
      send_to_char("List what?\n\rSyntax: vend list <all | mine | private>\n\r", ch);
    }
    return eSUCCESS;
  }

  /*SELL*/
  if(!strcmp(buf, "sell"))
  {
    argument = one_argument(argument, buf);
    if(!*buf)
    {
      send_to_char("Sell what?\n\rSyntax: vend sell <item> <price> [person | advertise]\n\r", ch);
      return eSUCCESS;
    }  
    obj = get_obj_in_list_vis(ch, buf, ch->carrying);
    if(!obj)
    {
      send_to_char("You don't seem to have that item.\n\rSyntax: vend sell <item> <price> [person | advertise]\n\r", ch);
      return eSUCCESS;
    }
    argument = one_argument(argument, buf);
    if(!*buf)
    {
       send_to_char("How much do you want to sell it for?\n\rSyntax: vend sell <item> <price> [person | advertise]\n\r", ch);
       return eSUCCESS;
    }
    price = atoi(buf);
    if(price < 1000)
    {
      send_to_char("Minimum sell price is 1000 coins!\n\r", ch);
      return eSUCCESS;
    }
  
    argument = one_argument(argument, buf); //private name
    TheAuctionHouse.AddItem(ch, obj, price, buf);
    return eSUCCESS;    
  }

  /*IDENTIFY*/
  if(!strcmp(buf, "identify"))
  {
    argument = one_argument(argument, buf);
    if(!*buf)
    {
       send_to_char("Identify what?\n\rSyntax: vend identify <ticket>\n\r", ch);
       return eSUCCESS;
    }
    TheAuctionHouse.Identify(ch, atoi(buf));
    return eSUCCESS;
  }


  /*ADD ROOM*/
  if(GET_LEVEL(ch) >= 108 && !strcmp(buf, "addroom"))
  {
    argument = one_argument(argument, buf);
    if(!*buf)
    {
      send_to_char("Add what room?\n\rSyntax: vend addroom <vnum>\n\r", ch);
      return eSUCCESS;
    }  
    TheAuctionHouse.AddRoom(ch, atoi(buf));
    return eSUCCESS;
  }

  /*REMOVE ROOM*/
  if(GET_LEVEL(ch) >= 108 && !strcmp(buf, "removeroom"))
  {
    argument = one_argument(argument, buf);
    if(!*buf)
    {
      send_to_char("Remove what room?\n\rSyntax: vend removeroom <vnum>\n\r", ch);
      return eSUCCESS;
    }  
    TheAuctionHouse.RemoveRoom(ch, atoi(buf));
    return eSUCCESS;
  }

  /*LIST ROOMS*/
  if(GET_LEVEL(ch) >= 108 && !strcmp(buf, "listrooms"))
  {
    TheAuctionHouse.ListRooms(ch);
    return eSUCCESS;
  }


  send_to_char("Do what?\n\rSyntax: vend <buy | sell | list | cancel | collect | search | identify>\n\r", ch);
  return eSUCCESS;
}
