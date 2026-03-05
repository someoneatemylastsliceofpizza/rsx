# Datatable (`dtbl`)

## Preview
Table

## Export Formats

### CSV
Data is exported as a Comma-Separated Values (CSV) file, with an additional metadata row added to the end of the file.

This metadata row indicates the data type contained by the column, as expected and read by the game. This is needed for recreating the asset correctly with [RePak](https://github.com/r-ex/RePak), and will always be present, even if there are no other data rows in the table.

Possible types:
- bool - true/false
- int - 32-bit integer
- float - Single-precision floating point number
- vector - float3, format `<f1, f2, f3>`
- string
- asset - String containing the path of an asset
- asset_noprecache - String containing the path of an asset that should not be precached

Example:
```csv
"levelIndex","temp","xpPerLevel","reward","rewardQty","premium","notes","notes2","rewardTier","isChaseReward"
0,"1",10,"settings/itemflav/pack/cosmetic_rare.rpak",1,false,"","",0,false
1,"2",10,"settings/itemflav/grx_currency/crafting.rpak",25,false,"","",0,false
2,"3",10,"settings/itemflav/voucher/store_offers/bp_stars_1.rpak",10,false,"","",0,false
3,"4",10,"settings/itemflav/grx_currency/crafting.rpak",25,false,"","",0,false
4,"5",10,"settings/itemflav/pack/thematic_horizon.rpak",1,false,"","",0,false
5,"6",10,"settings/itemflav/voucher/store_offers/bp_stars_1.rpak",10,false,"","",0,false
6,"7",10,"settings/itemflav/grx_currency/crafting.rpak",25,false,"","",0,false
7,"8",10,"settings/itemflav/pack/cosmetic_rare.rpak",1,false,"","",0,false
8,"9",10,"settings/itemflav/grx_currency/crafting.rpak",25,false,"","",0,false
9,"10",10,"settings/itemflav/pack/thematic_lgnd_submachine_gun.rpak",1,false,"","",0,true
10,"11",10,"settings/itemflav/pack/thematic_horizon.rpak",1,false,"","",0,false
11,"12",10,"settings/itemflav/grx_currency/crafting.rpak",25,false,"","",0,false
11,"12",0,"settings/itemflav/voucher/store_offers/bp_stars_1.rpak",10,false,"","",0,false
12,"13",10,"settings/itemflav/grx_currency/crafting.rpak",25,false,"","",0,false
13,"14",10,"settings/itemflav/voucher/store_offers/bp_stars_1.rpak",10,false,"","",0,false
14,"15",10,"settings/itemflav/pack/thematic_horizon.rpak",1,false,"","",0,false
15,"16",10,"settings/itemflav/grx_currency/crafting.rpak",25,false,"","",0,false
16,"17",10,"settings/itemflav/pack/cosmetic_rare.rpak",1,false,"","",0,false
17,"18",10,"settings/itemflav/voucher/store_offers/bp_stars_1.rpak",10,false,"","",0,false
18,"19",10,"settings/itemflav/grx_currency/crafting.rpak",25,false,"","",0,false
19,"20",10,"settings/itemflav/pack/legend_pass_lgnd_horizon_skin.rpak",1,false,"","",0,true
"int","string","int","asset","int","bool","string","string","int","bool"
```
