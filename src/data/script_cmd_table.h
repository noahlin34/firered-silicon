/* Auto-generated from data/script_cmd_table.inc */
#include "global.h"
#include "script.h"

bool8 ScrCmd_addcoins(struct ScriptContext *ctx);
bool8 ScrCmd_adddecoration(struct ScriptContext *ctx);
bool8 ScrCmd_addelevmenuitem(struct ScriptContext *ctx);
bool8 ScrCmd_additem(struct ScriptContext *ctx);
bool8 ScrCmd_addmoney(struct ScriptContext *ctx);
bool8 ScrCmd_addobject(struct ScriptContext *ctx);
bool8 ScrCmd_addobjectat(struct ScriptContext *ctx);
bool8 ScrCmd_addpcitem(struct ScriptContext *ctx);
bool8 ScrCmd_addvar(struct ScriptContext *ctx);
bool8 ScrCmd_animateflash(struct ScriptContext *ctx);
bool8 ScrCmd_applymovement(struct ScriptContext *ctx);
bool8 ScrCmd_applymovementat(struct ScriptContext *ctx);
bool8 ScrCmd_braillemessage(struct ScriptContext *ctx);
bool8 ScrCmd_bufferboxname(struct ScriptContext *ctx);
bool8 ScrCmd_bufferdecorationname(struct ScriptContext *ctx);
bool8 ScrCmd_bufferitemname(struct ScriptContext *ctx);
bool8 ScrCmd_bufferitemnameplural(struct ScriptContext *ctx);
bool8 ScrCmd_bufferleadmonspeciesname(struct ScriptContext *ctx);
bool8 ScrCmd_buffermovename(struct ScriptContext *ctx);
bool8 ScrCmd_buffernumberstring(struct ScriptContext *ctx);
bool8 ScrCmd_bufferpartymonnick(struct ScriptContext *ctx);
bool8 ScrCmd_bufferspeciesname(struct ScriptContext *ctx);
bool8 ScrCmd_bufferstdstring(struct ScriptContext *ctx);
bool8 ScrCmd_bufferstring(struct ScriptContext *ctx);
bool8 ScrCmd_call(struct ScriptContext *ctx);
bool8 ScrCmd_call_if(struct ScriptContext *ctx);
bool8 ScrCmd_callnative(struct ScriptContext *ctx);
bool8 ScrCmd_callstd(struct ScriptContext *ctx);
bool8 ScrCmd_callstd_if(struct ScriptContext *ctx);
bool8 ScrCmd_checkcoins(struct ScriptContext *ctx);
bool8 ScrCmd_checkdecor(struct ScriptContext *ctx);
bool8 ScrCmd_checkdecorspace(struct ScriptContext *ctx);
bool8 ScrCmd_checkflag(struct ScriptContext *ctx);
bool8 ScrCmd_checkitem(struct ScriptContext *ctx);
bool8 ScrCmd_checkitemspace(struct ScriptContext *ctx);
bool8 ScrCmd_checkitemtype(struct ScriptContext *ctx);
bool8 ScrCmd_checkmoney(struct ScriptContext *ctx);
bool8 ScrCmd_checkmonmodernfatefulencounter(struct ScriptContext *ctx);
bool8 ScrCmd_checkpartymove(struct ScriptContext *ctx);
bool8 ScrCmd_checkpcitem(struct ScriptContext *ctx);
bool8 ScrCmd_checkplayergender(struct ScriptContext *ctx);
bool8 ScrCmd_checktrainerflag(struct ScriptContext *ctx);
bool8 ScrCmd_choosecontestmon(struct ScriptContext *ctx);
bool8 ScrCmd_clearflag(struct ScriptContext *ctx);
bool8 ScrCmd_cleartrainerflag(struct ScriptContext *ctx);
bool8 ScrCmd_closedoor(struct ScriptContext *ctx);
bool8 ScrCmd_closemessage(struct ScriptContext *ctx);
bool8 ScrCmd_compare_local_to_local(struct ScriptContext *ctx);
bool8 ScrCmd_compare_local_to_ptr(struct ScriptContext *ctx);
bool8 ScrCmd_compare_local_to_value(struct ScriptContext *ctx);
bool8 ScrCmd_compare_ptr_to_local(struct ScriptContext *ctx);
bool8 ScrCmd_compare_ptr_to_ptr(struct ScriptContext *ctx);
bool8 ScrCmd_compare_ptr_to_value(struct ScriptContext *ctx);
bool8 ScrCmd_compare_var_to_value(struct ScriptContext *ctx);
bool8 ScrCmd_compare_var_to_var(struct ScriptContext *ctx);
bool8 ScrCmd_comparestat(struct ScriptContext *ctx);
bool8 ScrCmd_contestlinktransfer(struct ScriptContext *ctx);
bool8 ScrCmd_copybyte(struct ScriptContext *ctx);
bool8 ScrCmd_copylocal(struct ScriptContext *ctx);
bool8 ScrCmd_copyobjectxytoperm(struct ScriptContext *ctx);
bool8 ScrCmd_copyvar(struct ScriptContext *ctx);
bool8 ScrCmd_createvobject(struct ScriptContext *ctx);
bool8 ScrCmd_delay(struct ScriptContext *ctx);
bool8 ScrCmd_dofieldeffect(struct ScriptContext *ctx);
bool8 ScrCmd_dotimebasedevents(struct ScriptContext *ctx);
bool8 ScrCmd_dotrainerbattle(struct ScriptContext *ctx);
bool8 ScrCmd_doweather(struct ScriptContext *ctx);
bool8 ScrCmd_dowildbattle(struct ScriptContext *ctx);
bool8 ScrCmd_drawbox(struct ScriptContext *ctx);
bool8 ScrCmd_drawboxtext(struct ScriptContext *ctx);
bool8 ScrCmd_end(struct ScriptContext *ctx);
bool8 ScrCmd_endram(struct ScriptContext *ctx);
bool8 ScrCmd_erasebox(struct ScriptContext *ctx);
bool8 ScrCmd_faceplayer(struct ScriptContext *ctx);
bool8 ScrCmd_fadedefaultbgm(struct ScriptContext *ctx);
bool8 ScrCmd_fadeinbgm(struct ScriptContext *ctx);
bool8 ScrCmd_fadenewbgm(struct ScriptContext *ctx);
bool8 ScrCmd_fadeoutbgm(struct ScriptContext *ctx);
bool8 ScrCmd_fadescreen(struct ScriptContext *ctx);
bool8 ScrCmd_fadescreenspeed(struct ScriptContext *ctx);
bool8 ScrCmd_getbraillestringwidth(struct ScriptContext *ctx);
bool8 ScrCmd_getpartysize(struct ScriptContext *ctx);
bool8 ScrCmd_getplayerxy(struct ScriptContext *ctx);
bool8 ScrCmd_getpokenewsactive(struct ScriptContext *ctx);
bool8 ScrCmd_gettime(struct ScriptContext *ctx);
bool8 ScrCmd_giveegg(struct ScriptContext *ctx);
bool8 ScrCmd_givemon(struct ScriptContext *ctx);
bool8 ScrCmd_goto(struct ScriptContext *ctx);
bool8 ScrCmd_goto_if(struct ScriptContext *ctx);
bool8 ScrCmd_gotobeatenscript(struct ScriptContext *ctx);
bool8 ScrCmd_gotonative(struct ScriptContext *ctx);
bool8 ScrCmd_gotopostbattlescript(struct ScriptContext *ctx);
bool8 ScrCmd_gotostd(struct ScriptContext *ctx);
bool8 ScrCmd_gotostd_if(struct ScriptContext *ctx);
bool8 ScrCmd_hidecoinsbox(struct ScriptContext *ctx);
bool8 ScrCmd_hidemoneybox(struct ScriptContext *ctx);
bool8 ScrCmd_hidemonpic(struct ScriptContext *ctx);
bool8 ScrCmd_hideobjectat(struct ScriptContext *ctx);
bool8 ScrCmd_incrementgamestat(struct ScriptContext *ctx);
bool8 ScrCmd_initclock(struct ScriptContext *ctx);
bool8 ScrCmd_loadbyte(struct ScriptContext *ctx);
bool8 ScrCmd_loadbytefromptr(struct ScriptContext *ctx);
bool8 ScrCmd_loadhelp(struct ScriptContext *ctx);
bool8 ScrCmd_loadword(struct ScriptContext *ctx);
bool8 ScrCmd_lock(struct ScriptContext *ctx);
bool8 ScrCmd_lockall(struct ScriptContext *ctx);
bool8 ScrCmd_message(struct ScriptContext *ctx);
bool8 ScrCmd_messageautoscroll(struct ScriptContext *ctx);
bool8 ScrCmd_multichoice(struct ScriptContext *ctx);
bool8 ScrCmd_multichoicedefault(struct ScriptContext *ctx);
bool8 ScrCmd_multichoicegrid(struct ScriptContext *ctx);
bool8 ScrCmd_nop(struct ScriptContext *ctx);
bool8 ScrCmd_nop1(struct ScriptContext *ctx);
bool8 ScrCmd_normalmsg(struct ScriptContext *ctx);
bool8 ScrCmd_opendoor(struct ScriptContext *ctx);
bool8 ScrCmd_playbgm(struct ScriptContext *ctx);
bool8 ScrCmd_playfanfare(struct ScriptContext *ctx);
bool8 ScrCmd_playmoncry(struct ScriptContext *ctx);
bool8 ScrCmd_playse(struct ScriptContext *ctx);
bool8 ScrCmd_playslotmachine(struct ScriptContext *ctx);
bool8 ScrCmd_pokemart(struct ScriptContext *ctx);
bool8 ScrCmd_pokemartdecoration(struct ScriptContext *ctx);
bool8 ScrCmd_pokemartdecoration2(struct ScriptContext *ctx);
bool8 ScrCmd_random(struct ScriptContext *ctx);
bool8 ScrCmd_release(struct ScriptContext *ctx);
bool8 ScrCmd_releaseall(struct ScriptContext *ctx);
bool8 ScrCmd_removecoins(struct ScriptContext *ctx);
bool8 ScrCmd_removedecoration(struct ScriptContext *ctx);
bool8 ScrCmd_removeitem(struct ScriptContext *ctx);
bool8 ScrCmd_removemoney(struct ScriptContext *ctx);
bool8 ScrCmd_removeobject(struct ScriptContext *ctx);
bool8 ScrCmd_removeobjectat(struct ScriptContext *ctx);
bool8 ScrCmd_resetobjectsubpriority(struct ScriptContext *ctx);
bool8 ScrCmd_resetweather(struct ScriptContext *ctx);
bool8 ScrCmd_return(struct ScriptContext *ctx);
bool8 ScrCmd_returnram(struct ScriptContext *ctx);
bool8 ScrCmd_savebgm(struct ScriptContext *ctx);
bool8 ScrCmd_setberrytree(struct ScriptContext *ctx);
bool8 ScrCmd_setdivewarp(struct ScriptContext *ctx);
bool8 ScrCmd_setdoorclosed(struct ScriptContext *ctx);
bool8 ScrCmd_setdooropen(struct ScriptContext *ctx);
bool8 ScrCmd_setdynamicwarp(struct ScriptContext *ctx);
bool8 ScrCmd_setescapewarp(struct ScriptContext *ctx);
bool8 ScrCmd_setfieldeffectargument(struct ScriptContext *ctx);
bool8 ScrCmd_setflag(struct ScriptContext *ctx);
bool8 ScrCmd_setflashlevel(struct ScriptContext *ctx);
bool8 ScrCmd_setholewarp(struct ScriptContext *ctx);
bool8 ScrCmd_setmaplayoutindex(struct ScriptContext *ctx);
bool8 ScrCmd_setmetatile(struct ScriptContext *ctx);
bool8 ScrCmd_setmonmetlocation(struct ScriptContext *ctx);
bool8 ScrCmd_setmonmodernfatefulencounter(struct ScriptContext *ctx);
bool8 ScrCmd_setmonmove(struct ScriptContext *ctx);
bool8 ScrCmd_setmysteryeventstatus(struct ScriptContext *ctx);
bool8 ScrCmd_setobjectmovementtype(struct ScriptContext *ctx);
bool8 ScrCmd_setobjectsubpriority(struct ScriptContext *ctx);
bool8 ScrCmd_setobjectxy(struct ScriptContext *ctx);
bool8 ScrCmd_setobjectxyperm(struct ScriptContext *ctx);
bool8 ScrCmd_setorcopyvar(struct ScriptContext *ctx);
bool8 ScrCmd_setptr(struct ScriptContext *ctx);
bool8 ScrCmd_setptrbyte(struct ScriptContext *ctx);
bool8 ScrCmd_setrespawn(struct ScriptContext *ctx);
bool8 ScrCmd_setstepcallback(struct ScriptContext *ctx);
bool8 ScrCmd_settrainerflag(struct ScriptContext *ctx);
bool8 ScrCmd_setvaddress(struct ScriptContext *ctx);
bool8 ScrCmd_setvar(struct ScriptContext *ctx);
bool8 ScrCmd_setwarp(struct ScriptContext *ctx);
bool8 ScrCmd_setweather(struct ScriptContext *ctx);
bool8 ScrCmd_setwildbattle(struct ScriptContext *ctx);
bool8 ScrCmd_setworldmapflag(struct ScriptContext *ctx);
bool8 ScrCmd_showcoinsbox(struct ScriptContext *ctx);
bool8 ScrCmd_showcontestpainting(struct ScriptContext *ctx);
bool8 ScrCmd_showcontestresults(struct ScriptContext *ctx);
bool8 ScrCmd_showelevmenu(struct ScriptContext *ctx);
bool8 ScrCmd_showmoneybox(struct ScriptContext *ctx);
bool8 ScrCmd_showmonpic(struct ScriptContext *ctx);
bool8 ScrCmd_showobjectat(struct ScriptContext *ctx);
bool8 ScrCmd_signmsg(struct ScriptContext *ctx);
bool8 ScrCmd_special(struct ScriptContext *ctx);
bool8 ScrCmd_specialvar(struct ScriptContext *ctx);
bool8 ScrCmd_startcontest(struct ScriptContext *ctx);
bool8 ScrCmd_subvar(struct ScriptContext *ctx);
bool8 ScrCmd_textcolor(struct ScriptContext *ctx);
bool8 ScrCmd_trainerbattle(struct ScriptContext *ctx);
bool8 ScrCmd_trywondercardscript(struct ScriptContext *ctx);
bool8 ScrCmd_turnobject(struct ScriptContext *ctx);
bool8 ScrCmd_turnvobject(struct ScriptContext *ctx);
bool8 ScrCmd_unloadhelp(struct ScriptContext *ctx);
bool8 ScrCmd_updatecoinsbox(struct ScriptContext *ctx);
bool8 ScrCmd_updatemoneybox(struct ScriptContext *ctx);
bool8 ScrCmd_vbuffermessage(struct ScriptContext *ctx);
bool8 ScrCmd_vbufferstring(struct ScriptContext *ctx);
bool8 ScrCmd_vcall(struct ScriptContext *ctx);
bool8 ScrCmd_vcall_if(struct ScriptContext *ctx);
bool8 ScrCmd_vgoto(struct ScriptContext *ctx);
bool8 ScrCmd_vgoto_if(struct ScriptContext *ctx);
bool8 ScrCmd_vmessage(struct ScriptContext *ctx);
bool8 ScrCmd_waitbuttonpress(struct ScriptContext *ctx);
bool8 ScrCmd_waitdooranim(struct ScriptContext *ctx);
bool8 ScrCmd_waitfanfare(struct ScriptContext *ctx);
bool8 ScrCmd_waitfieldeffect(struct ScriptContext *ctx);
bool8 ScrCmd_waitmessage(struct ScriptContext *ctx);
bool8 ScrCmd_waitmoncry(struct ScriptContext *ctx);
bool8 ScrCmd_waitmovement(struct ScriptContext *ctx);
bool8 ScrCmd_waitmovementat(struct ScriptContext *ctx);
bool8 ScrCmd_waitse(struct ScriptContext *ctx);
bool8 ScrCmd_waitstate(struct ScriptContext *ctx);
bool8 ScrCmd_warp(struct ScriptContext *ctx);
bool8 ScrCmd_warpdoor(struct ScriptContext *ctx);
bool8 ScrCmd_warphole(struct ScriptContext *ctx);
bool8 ScrCmd_warpsilent(struct ScriptContext *ctx);
bool8 ScrCmd_warpspinenter(struct ScriptContext *ctx);
bool8 ScrCmd_warpteleport(struct ScriptContext *ctx);
bool8 ScrCmd_yesnobox(struct ScriptContext *ctx);

const ScrCmdFunc gScriptCmdTable[] = {
    ScrCmd_nop,
    ScrCmd_nop1,
    ScrCmd_end,
    ScrCmd_return,
    ScrCmd_call,
    ScrCmd_goto,
    ScrCmd_goto_if,
    ScrCmd_call_if,
    ScrCmd_gotostd,
    ScrCmd_callstd,
    ScrCmd_gotostd_if,
    ScrCmd_callstd_if,
    ScrCmd_returnram,
    ScrCmd_endram,
    ScrCmd_setmysteryeventstatus,
    ScrCmd_loadword,
    ScrCmd_loadbyte,
    ScrCmd_setptr,
    ScrCmd_loadbytefromptr,
    ScrCmd_setptrbyte,
    ScrCmd_copylocal,
    ScrCmd_copybyte,
    ScrCmd_setvar,
    ScrCmd_addvar,
    ScrCmd_subvar,
    ScrCmd_copyvar,
    ScrCmd_setorcopyvar,
    ScrCmd_compare_local_to_local,
    ScrCmd_compare_local_to_value,
    ScrCmd_compare_local_to_ptr,
    ScrCmd_compare_ptr_to_local,
    ScrCmd_compare_ptr_to_value,
    ScrCmd_compare_ptr_to_ptr,
    ScrCmd_compare_var_to_value,
    ScrCmd_compare_var_to_var,
    ScrCmd_callnative,
    ScrCmd_gotonative,
    ScrCmd_special,
    ScrCmd_specialvar,
    ScrCmd_waitstate,
    ScrCmd_delay,
    ScrCmd_setflag,
    ScrCmd_clearflag,
    ScrCmd_checkflag,
    ScrCmd_initclock,
    ScrCmd_dotimebasedevents,
    ScrCmd_gettime,
    ScrCmd_playse,
    ScrCmd_waitse,
    ScrCmd_playfanfare,
    ScrCmd_waitfanfare,
    ScrCmd_playbgm,
    ScrCmd_savebgm,
    ScrCmd_fadedefaultbgm,
    ScrCmd_fadenewbgm,
    ScrCmd_fadeoutbgm,
    ScrCmd_fadeinbgm,
    ScrCmd_warp,
    ScrCmd_warpsilent,
    ScrCmd_warpdoor,
    ScrCmd_warphole,
    ScrCmd_warpteleport,
    ScrCmd_setwarp,
    ScrCmd_setdynamicwarp,
    ScrCmd_setdivewarp,
    ScrCmd_setholewarp,
    ScrCmd_getplayerxy,
    ScrCmd_getpartysize,
    ScrCmd_additem,
    ScrCmd_removeitem,
    ScrCmd_checkitemspace,
    ScrCmd_checkitem,
    ScrCmd_checkitemtype,
    ScrCmd_addpcitem,
    ScrCmd_checkpcitem,
    ScrCmd_adddecoration,
    ScrCmd_removedecoration,
    ScrCmd_checkdecor,
    ScrCmd_checkdecorspace,
    ScrCmd_applymovement,
    ScrCmd_applymovementat,
    ScrCmd_waitmovement,
    ScrCmd_waitmovementat,
    ScrCmd_removeobject,
    ScrCmd_removeobjectat,
    ScrCmd_addobject,
    ScrCmd_addobjectat,
    ScrCmd_setobjectxy,
    ScrCmd_showobjectat,
    ScrCmd_hideobjectat,
    ScrCmd_faceplayer,
    ScrCmd_turnobject,
    ScrCmd_trainerbattle,
    ScrCmd_dotrainerbattle,
    ScrCmd_gotopostbattlescript,
    ScrCmd_gotobeatenscript,
    ScrCmd_checktrainerflag,
    ScrCmd_settrainerflag,
    ScrCmd_cleartrainerflag,
    ScrCmd_setobjectxyperm,
    ScrCmd_copyobjectxytoperm,
    ScrCmd_setobjectmovementtype,
    ScrCmd_waitmessage,
    ScrCmd_message,
    ScrCmd_closemessage,
    ScrCmd_lockall,
    ScrCmd_lock,
    ScrCmd_releaseall,
    ScrCmd_release,
    ScrCmd_waitbuttonpress,
    ScrCmd_yesnobox,
    ScrCmd_multichoice,
    ScrCmd_multichoicedefault,
    ScrCmd_multichoicegrid,
    ScrCmd_drawbox,
    ScrCmd_erasebox,
    ScrCmd_drawboxtext,
    ScrCmd_showmonpic,
    ScrCmd_hidemonpic,
    ScrCmd_showcontestpainting,
    ScrCmd_braillemessage,
    ScrCmd_givemon,
    ScrCmd_giveegg,
    ScrCmd_setmonmove,
    ScrCmd_checkpartymove,
    ScrCmd_bufferspeciesname,
    ScrCmd_bufferleadmonspeciesname,
    ScrCmd_bufferpartymonnick,
    ScrCmd_bufferitemname,
    ScrCmd_bufferdecorationname,
    ScrCmd_buffermovename,
    ScrCmd_buffernumberstring,
    ScrCmd_bufferstdstring,
    ScrCmd_bufferstring,
    ScrCmd_pokemart,
    ScrCmd_pokemartdecoration,
    ScrCmd_pokemartdecoration2,
    ScrCmd_playslotmachine,
    ScrCmd_setberrytree,
    ScrCmd_choosecontestmon,
    ScrCmd_startcontest,
    ScrCmd_showcontestresults,
    ScrCmd_contestlinktransfer,
    ScrCmd_random,
    ScrCmd_addmoney,
    ScrCmd_removemoney,
    ScrCmd_checkmoney,
    ScrCmd_showmoneybox,
    ScrCmd_hidemoneybox,
    ScrCmd_updatemoneybox,
    ScrCmd_getpokenewsactive,
    ScrCmd_fadescreen,
    ScrCmd_fadescreenspeed,
    ScrCmd_setflashlevel,
    ScrCmd_animateflash,
    ScrCmd_messageautoscroll,
    ScrCmd_dofieldeffect,
    ScrCmd_setfieldeffectargument,
    ScrCmd_waitfieldeffect,
    ScrCmd_setrespawn,
    ScrCmd_checkplayergender,
    ScrCmd_playmoncry,
    ScrCmd_setmetatile,
    ScrCmd_resetweather,
    ScrCmd_setweather,
    ScrCmd_doweather,
    ScrCmd_setstepcallback,
    ScrCmd_setmaplayoutindex,
    ScrCmd_setobjectsubpriority,
    ScrCmd_resetobjectsubpriority,
    ScrCmd_createvobject,
    ScrCmd_turnvobject,
    ScrCmd_opendoor,
    ScrCmd_closedoor,
    ScrCmd_waitdooranim,
    ScrCmd_setdooropen,
    ScrCmd_setdoorclosed,
    ScrCmd_addelevmenuitem,
    ScrCmd_showelevmenu,
    ScrCmd_checkcoins,
    ScrCmd_addcoins,
    ScrCmd_removecoins,
    ScrCmd_setwildbattle,
    ScrCmd_dowildbattle,
    ScrCmd_setvaddress,
    ScrCmd_vgoto,
    ScrCmd_vcall,
    ScrCmd_vgoto_if,
    ScrCmd_vcall_if,
    ScrCmd_vmessage,
    ScrCmd_vbuffermessage,
    ScrCmd_vbufferstring,
    ScrCmd_showcoinsbox,
    ScrCmd_hidecoinsbox,
    ScrCmd_updatecoinsbox,
    ScrCmd_incrementgamestat,
    ScrCmd_setescapewarp,
    ScrCmd_waitmoncry,
    ScrCmd_bufferboxname,
    ScrCmd_textcolor,
    ScrCmd_loadhelp,
    ScrCmd_unloadhelp,
    ScrCmd_signmsg,
    ScrCmd_normalmsg,
    ScrCmd_comparestat,
    ScrCmd_setmonmodernfatefulencounter,
    ScrCmd_checkmonmodernfatefulencounter,
    ScrCmd_trywondercardscript,
    ScrCmd_setworldmapflag,
    ScrCmd_warpspinenter,
    ScrCmd_setmonmetlocation,
    ScrCmd_getbraillestringwidth,
    ScrCmd_bufferitemnameplural,
    ScrCmd_nop,
};

const ScrCmdFunc gScriptCmdTableEnd[] = { NULL };
