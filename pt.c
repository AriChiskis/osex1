#include<stdlib.h>
#include<stdio.h>
#include"os.h"

//SOME DEFINITIONS FOR CONVINIENCE :)
//////////////////////////////////////
// Define the number of page table levels
#define NUM_PT_LEVELS 5
#define LAST_TABLE 4
// Define the size of the virtual page number
#define VPN_MASK 0X0001FFFFFFFFFFFF
// Define the number of slices
#define SLICE 9
#define SLICE_MASK 0x1FF
// Define 12 bit address shifter
#define NUMBER_TO_ADDRESS_SHIFTER 12
// Define the number 1 as ONE
#define ONE 1
// Define VALID as 1
#define VALID 1
// Define LAST_BIT_0
#define LAST_BIT_0 ~((uint64_t)1)
//////////////////////////////////////////////////////////////////////////////////
uint64_t frame_number_to_physical_row_address(uint64_t frame_num){
  return  frame_num << NUMBER_TO_ADDRESS_SHIFTER;
}

uint64_t physical_row_address_to_frame_number(uint64_t pysical_row){
  return pysical_row >> NUMBER_TO_ADDRESS_SHIFTER;
}

uint64_t create_valid_row_in_pt(uint64_t frame_addr){
  uint64_t valid_row = frame_number_to_physical_row_address(frame_addr);
  valid_row = valid_row | VALID;
  return valid_row;

}

uint64_t change_valid_bit_to_0(uint64_t addr){
  return addr & LAST_BIT_0;
}

uint64_t convert_frame_row_to_physical_addr(uint64_t addr){
  uint64_t addr_num = addr;
  addr_num = addr_num >> NUMBER_TO_ADDRESS_SHIFTER;
  addr_num = addr_num << NUMBER_TO_ADDRESS_SHIFTER;
  return addr_num;
}
/////////////////////////////////////////////////////////////////////////////

int vpn_map_exists(uint64_t* current_pt, uint64_t address){
//check the valid bit if its 1 or 0, if 1 then map exists, else deont exists
  if ((current_pt[address]  & ONE) == VALID ){
    return 1; // vpn_map_exists,
    }
  else { // valid bit is 0 , so this is an invalid page :(
    return 0;
    }
}


void case_ppn_is_no_mapping(uint64_t* root_pt,uint64_t* vpn_address){
    uint64_t* page_table_ptr = root_pt;
    for(int i = 0; i < NUM_PT_LEVELS - 1;i++){
      if (vpn_map_exists(page_table_ptr,vpn_address[i])){
        uint64_t frame_row = page_table_ptr[vpn_address[i]];
        page_table_ptr = (uint64_t*) phys_to_virt(convert_frame_row_to_physical_addr(frame_row));
                }
      else {
        // vpn_map doesnt exists so we have reached an unvalid PageTable so we can return
        return;
                }
      }
    // HEY, is youre here, it means that you you have reached the LAST TABLE (bravo)
    // all i as kyou is to change the valid bit to 0 in the page_table_entry that you have been mapped cause its an invalid page :( (ppn == NO_MAPPING)
    uint64_t pte_last_table = page_table_ptr[vpn_address[LAST_TABLE]];
    page_table_ptr[vpn_address[LAST_TABLE]] = change_valid_bit_to_0(pte_last_table);
    return ;
  }

void case_ppn_exists(uint64_t* root_pt,uint64_t* vpn_address,uint64_t ppn){
    uint64_t* page_table_ptr = root_pt;
    uint64_t new_page_table_physical_adrress;
    for(int i = 0; i < NUM_PT_LEVELS - 1;i++){
      if (vpn_map_exists(page_table_ptr,vpn_address[i])){
          // vpn_map_exists, no problem , go to the next table address
          new_page_table_physical_adrress = convert_frame_row_to_physical_addr(page_table_ptr[vpn_address[i]]);
    }
    else{
        // vpn_map doesnt exists so we call alloc_page and we create a new page table ! (coool)
        //we will get a uint64_t number, but only the rightest bits are important!we need to make it a valid row in the page table, lets do it
        uint64_t new_page_table_address =  alloc_page_frame();
        new_page_table_physical_adrress = frame_number_to_physical_row_address(new_page_table_address);
        page_table_ptr[vpn_address[i]] = create_valid_row_in_pt(new_page_table_address); // set the address of the next
        }
        // lets go to the next page table.
        page_table_ptr = (uint64_t*) phys_to_virt(new_page_table_physical_adrress);
    }
// if you reached here, you just need to put the physical page in the proper PTE.
    page_table_ptr[vpn_address[LAST_TABLE]] = create_valid_row_in_pt(ppn);

    return ;
              }




// we will get an array of all the addresses in the nested page tables (we will have 5)
uint64_t* vpn_addresses(uint64_t virtual_addr){
  uint64_t *vpn_adrr = malloc(NUM_PT_LEVELS*sizeof(uint64_t));
    if (vpn_adrr ==NULL){
        printf("failed allocate vpn addresses array :(\n");
        exit(1);
      }
    uint64_t curr_vpn_part = (virtual_addr )& VPN_MASK;
    for(int i = 0; i < NUM_PT_LEVELS;i++){
        vpn_adrr[NUM_PT_LEVELS - i - 1] = curr_vpn_part & SLICE_MASK;
        curr_vpn_part = curr_vpn_part >>SLICE;
}
return vpn_adrr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn){
  uint64_t* vpn_address = vpn_addresses(vpn);
  uint64_t* root_pt = phys_to_virt(frame_number_to_physical_row_address(pt));
  //case ppn == NO_MAPPING;
  if (ppn == NO_MAPPING) {
    case_ppn_is_no_mapping(root_pt,vpn_address);
    }
  else {
  case_ppn_exists(root_pt,vpn_address,ppn);
  }
  free(vpn_address);
  return;
}


uint64_t page_table_query(uint64_t pt, uint64_t vpn){
  uint64_t* vpn_address = vpn_addresses(vpn);
  // set pointer to the root directory page table
  uint64_t* page_table_ptr =  (uint64_t*) phys_to_virt(frame_number_to_physical_row_address(pt));
  // start iterating over all the nested page tables
  for(int i = 0; i < NUM_PT_LEVELS - 1;i++){

      if (!(vpn_map_exists(page_table_ptr,vpn_address[i]))) {
        // There is no map to the current row in the page table (cause valid bit is 0)
        // so we return NO_MAPPING as required
          return NO_MAPPING;
            }
      // There is a mapping :) (valid bit in row is 1)
      // lets go to the next page table
      uint64_t next_page_table_address = convert_frame_row_to_physical_addr(page_table_ptr[vpn_address[i]]);
      page_table_ptr = (uint64_t*) phys_to_virt(next_page_table_address);
  }
    // Now we are in the last table (OMGGG)
    if (!(vpn_map_exists(page_table_ptr,vpn_address[LAST_TABLE]))){
        // There is no mapping finally in the last table
          return NO_MAPPING;
      }
    else {
          // There is a mapping ! we did it,
          //lets extract to ppn from the row in the page table
          uint64_t physical_row = page_table_ptr[vpn_address[LAST_TABLE]];
          uint64_t ppn = physical_row_address_to_frame_number(physical_row);
          return ppn;
    }
}

