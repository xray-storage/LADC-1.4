#include "pch_script.h"
#include "Actor_Flags.h"
#include "hudmanager.h"
#include "alife_space.h"
#include "hit.h"
#include "PHDestroyable.h"
#include "Car.h"
#include "xrserver_objects_alife_monsters.h"
#include "CameraLook.h"
#include "CameraFirstEye.h"
#include "effectorfall.h"
#include "EffectorBobbing.h"
//#include "clsid_game.h"
#include "ActorEffector.h"
#include "EffectorZoomInertion.h"
#include "SleepEffector.h"
//#include "character_info.h"
#include "CustomOutfit.h"
#include "actorcondition.h"
#include "UIGameCustom.h"

//#include "game_cl_base_weapon_usage_statistic.h"
//#include "../xr_input.h"
#include "Actor.h"
#include "ActorAnimation.h"
#include "actor_anim_defs.h"
#include "HudItem.h"
#include "ai_sounds.h"
#include "ai_space.h"
//#include "trade.h"
#include "inventory.h"
#include "Physics.h"
#include "level.h"
#include "GamePersistent.h"
#include "game_cl_base.h"
#include "game_cl_single.h"
//#include "xrmessages.h"
#include "string_table.h"
//#include "usablescriptobject.h"
#include "../cl_intersect.h"

//#include "ExtendedGeom.h"
#include "alife_registry_wrappers.h"
#include "../../Include/xrRender/KinematicsAnimated.h"
#include "../../Include/xrRender/Kinematics.h"
#include "artifact.h"
#include "CharacterPhysicsSupport.h"
#include "material_manager.h"
#include "IColisiondamageInfo.h"
#include "ui/UIMainIngameWnd.h"
//#include "ui/UIArtefactPanel.h"
//#include "map_manager.h"
#include "GameTaskManager.h"
#include "actor_memory.h"
#include "Script_Game_Object.h"
#include "Game_Object_Space.h"
#include "script_callback_ex.h"
#include "InventoryBox.h"
#include "mounted_turret.h"
#include "location_manager.h"
#include "player_hud.h"

Flags32			psActorFlags={0};

CActor::CActor() : CEntityAlive()
{
	encyclopedia_registry	= new CEncyclopediaRegistryWrapper();
	game_news_registry		= new CGameNewsRegistryWrapper();
	// Cameras
	cameras[eacFirstEye]	= new CCameraFirstEye(this, CCameraBase::flPositionRigid);
	cameras[eacFirstEye]->Load("actor_firsteye_cam");

	psActorFlags.set(AF_PSP, TRUE);

	cameras[eacLookAt]		= new CCameraLook2(this);
	cameras[eacLookAt]->Load("actor_look_cam");

	cameras[eacFreeLook]	= new CCameraLook(this);
	cameras[eacFreeLook]->Load("actor_free_cam");

	cam_active				= eacFirstEye;
	fPrevCamPos				= 0.0f;
	vPrevCamDir.set			(0.f,0.f,1.f);
	fCurAVelocity			= 0.0f;
	// ���������
	pCamBobbing				= 0;
	m_pSleepEffector		= NULL;
	m_pSleepEffectorPP		= NULL;

	r_torso.yaw				= 0;
	r_torso.pitch			= 0;
	r_torso.roll			= 0;
	r_torso_tgt_roll		= 0;
	r_model_yaw				= 0;
	r_model_yaw_delta		= 0;
	r_model_yaw_dest		= 0;

	b_DropActivated			= 0;
	f_DropPower				= 0.f;

	m_fRunFactor			= 2.f;
	m_fCrouchFactor			= 0.2f;
	m_fClimbFactor			= 1.f;
	m_fCamHeightFactor		= 0.87f;
	m_fRunFactorAdditional		= 0.f;
	m_fSprintFactorAdditional	= 0.f;
	m_fSprintFactor			= 4.f;
	m_fImmunityCoef			= 1.f;
	m_fDispersionCoef		= 1.f;
	m_fPriceFactor			= 1.f;
	m_fZoomInertCoef		= 1.f;

	m_fFallTime				= s_fFallTime;
	m_bAnimTorsoPlayed		= false;
	b_saveAllowed			= true;

	m_pPhysicsShell			= NULL;
	m_holder				= NULL;
	m_holderID				= u16(-1);

	inventory().SetBeltUseful(true);

	m_pPersonWeLookingAt	= NULL;
	m_pHolderWeLookingAt	= NULL;
	m_pObjectWeLookingAt	= NULL;
	m_bPickupMode			= false;

	pStatGraph				= NULL;
	m_pActorEffector		= NULL;
	m_bZoomAimingMode		= false;
	m_sDefaultObjAction		= NULL;

	m_icons_state.zero		 ();
	m_pUsableObject			= NULL;

	m_anims					= new SActorMotions();
	m_vehicle_anims			= new SActorVehicleAnims();
	m_entity_condition		= NULL;
	m_iLastHitterID			= u16(-1);
	m_iLastHittingWeaponID	= u16(-1);
	m_game_task_manager		= NULL;
	m_statistic_manager		= NULL;

	m_memory				= new CActorMemory(this);
	m_bOutBorder			= false;
	hit_probability			= 1.f;
	m_feel_touch_characters = 0;
	m_dwILastUpdateTime		= 0;
	m_location_manager		= new CLocationManager(this);
	m_current_torch			= 0;

	inventory().SetCurrentDetector(NULL);
}


CActor::~CActor()
{
	xr_delete				(m_location_manager);
	xr_delete				(m_memory);
	xr_delete				(encyclopedia_registry);
	xr_delete				(game_news_registry);

	for (int i=0; i<eacMaxCam; ++i) xr_delete(cameras[i]);

	m_HeavyBreathSnd.destroy();
	m_BloodSnd.destroy		();

	xr_delete				(m_pActorEffector);
	xr_delete				(m_pSleepEffector);
	xr_delete				(m_pPhysics_support);
	xr_delete				(m_anims);
	xr_delete				(m_vehicle_anims);
}

void CActor::reinit	()
{
	character_physics_support()->movement()->CreateCharacter		();
	character_physics_support()->movement()->SetPhysicsRefObject	(this);
	CEntityAlive::reinit						();
	CInventoryOwner::reinit						();

	character_physics_support()->in_Init		();
	material().reinit							();

	m_pUsableObject								= NULL;

	memory().reinit							();
	
	set_input_external_handler					(0);
	m_time_lock_accel							= 0;
}

void CActor::reload	(LPCSTR section)
{
	CEntityAlive::reload		(section);
	CInventoryOwner::reload		(section);
	material().reload			(section);
	CStepManager::reload		(section);

	memory().reload				(section);
	m_location_manager->reload	(section);
}

void CActor::Load	(LPCSTR section )
{
	inherited::Load				(section);
	material().Load				(section);
	CInventoryOwner::Load		(section);
	m_location_manager->Load	(section);

	OnDifficultyChanged		();
	//////////////////////////////////////////////////////////////////////////
	ISpatial*		self			=	smart_cast<ISpatial*> (this);
	if (self)	{
		self->spatial.type	|=	STYPE_VISIBLEFORAI;
		self->spatial.type	&= ~STYPE_REACTTOSOUND;
	}
	//////////////////////////////////////////////////////////////////////////

	// m_PhysicMovementControl: General
	//m_PhysicMovementControl->SetParent		(this);
	Fbox	bb;Fvector	vBOX_center,vBOX_size;
	// m_PhysicMovementControl: BOX
	vBOX_center= pSettings->r_fvector3	(section,"ph_box2_center"	);
	vBOX_size	= pSettings->r_fvector3	(section,"ph_box2_size"		);
	bb.set	(vBOX_center,vBOX_center); bb.grow(vBOX_size);
	character_physics_support()->movement()->SetBox		(2,bb);

	// m_PhysicMovementControl: BOX
	vBOX_center= pSettings->r_fvector3	(section,"ph_box1_center"	);
	vBOX_size	= pSettings->r_fvector3	(section,"ph_box1_size"		);
	bb.set	(vBOX_center,vBOX_center); bb.grow(vBOX_size);
	character_physics_support()->movement()->SetBox		(1,bb);

	// m_PhysicMovementControl: BOX
	vBOX_center= pSettings->r_fvector3	(section,"ph_box0_center"	);
	vBOX_size	= pSettings->r_fvector3	(section,"ph_box0_size"		);
	bb.set	(vBOX_center,vBOX_center); bb.grow(vBOX_size);
	character_physics_support()->movement()->SetBox		(0,bb);

	// m_PhysicMovementControl: Crash speed and mass
	float	cs_min		= pSettings->r_float	(section,"ph_crash_speed_min"	);
	float	cs_max		= pSettings->r_float	(section,"ph_crash_speed_max"	);
	float	mass		= pSettings->r_float	(section,"ph_mass"				);
	character_physics_support()->movement()->SetCrashSpeeds	(cs_min,cs_max);
	character_physics_support()->movement()->SetMass		(mass);
	if(pSettings->line_exist(section,"stalker_restrictor_radius"))
		character_physics_support()->movement()->SetActorRestrictorRadius(CPHCharacter::rtStalker,pSettings->r_float(section,"stalker_restrictor_radius"));
	if(pSettings->line_exist(section,"stalker_small_restrictor_radius"))
		character_physics_support()->movement()->SetActorRestrictorRadius(CPHCharacter::rtStalkerSmall,pSettings->r_float(section,"stalker_small_restrictor_radius"));
	if(pSettings->line_exist(section,"medium_monster_restrictor_radius"))
		character_physics_support()->movement()->SetActorRestrictorRadius(CPHCharacter::rtMonsterMedium,pSettings->r_float(section,"medium_monster_restrictor_radius"));
	character_physics_support()->movement()->Load(section);

	m_fWalkAccel				= pSettings->r_float(section,"walk_accel");	
	m_fJumpSpeed				= pSettings->r_float(section,"jump_speed");
	m_fRunFactor				= pSettings->r_float(section,"run_coef");
	m_fRunBackFactor			= pSettings->r_float(section,"run_back_coef");
	m_fWalkBackFactor			= pSettings->r_float(section,"walk_back_coef");
	m_fCrouchFactor				= pSettings->r_float(section,"crouch_coef");
	m_fClimbFactor				= pSettings->r_float(section,"climb_coef");
	m_fSprintFactor				= pSettings->r_float(section,"sprint_koef");

	m_fWalk_StrafeFactor		= READ_IF_EXISTS(pSettings, r_float, section, "walk_strafe_coef", 1.0f);
	m_fRun_StrafeFactor			= READ_IF_EXISTS(pSettings, r_float, section, "run_strafe_coef", 1.0f);

	m_fRunFactorAdditionalLimit		= READ_IF_EXISTS(pSettings, r_float, section, "artifact_run_speed_limit",		m_fRunFactor);
	m_fSprintFactorAdditionalLimit	= READ_IF_EXISTS(pSettings, r_float, section, "artifact_sprint_speed_limit",	m_fSprintFactor);
	m_fJumpFactorAdditionalLimit	= READ_IF_EXISTS(pSettings, r_float, section, "artifact_jump_speed_limit",		m_fJumpSpeed);

	m_fCamHeightFactor			= pSettings->r_float(section,"camera_height_factor");
	character_physics_support()->movement()		->SetJumpUpVelocity(m_fJumpSpeed);
	float AirControlParam		= pSettings->r_float	(section,"air_control_param"	);
	character_physics_support()->movement()		->SetAirControlParam(AirControlParam);

	m_fPickupInfoRadius	= pSettings->r_float(section,"pickup_info_radius");
	m_fSleepTimeFactor	= pSettings->r_float(section,"sleep_time_factor");

	character_physics_support()->in_Load		(section);
	
	//��������� ��������� ���������
	LoadSleepEffector		("sleep_effector");

	//��������� ��������� �������� firepoint
	m_vMissileOffset	= pSettings->r_fvector3(section,"missile_throw_offset");

	LPCSTR hit_snd_sect = pSettings->r_string(section,"hit_sounds");
	for(int hit_type=0; hit_type<(int)ALife::eHitTypeMax; ++hit_type)
	{
		LPCSTR hit_name = ALife::g_cafHitType2String((ALife::EHitType)hit_type);
		LPCSTR hit_snds = READ_IF_EXISTS(pSettings, r_string, hit_snd_sect, hit_name, "");
		int cnt = _GetItemCount(hit_snds);
		string128		tmp;
		VERIFY			(cnt!=0);
		for(int i=0; i<cnt;++i)
		{
			sndHit[hit_type].push_back		(ref_sound());
			sndHit[hit_type].back().create	(_GetItem(hit_snds,i,tmp),st_Effect,sg_SourceType);
		}
		char buf[256];

		::Sound->create		(sndDie[0],			strconcat(sizeof(buf),buf,*cName(),"\\die0"), st_Effect,SOUND_TYPE_MONSTER_DYING);
		::Sound->create		(sndDie[1],			strconcat(sizeof(buf),buf,*cName(),"\\die1"), st_Effect,SOUND_TYPE_MONSTER_DYING);
		::Sound->create		(sndDie[2],			strconcat(sizeof(buf),buf,*cName(),"\\die2"), st_Effect,SOUND_TYPE_MONSTER_DYING);
		::Sound->create		(sndDie[3],			strconcat(sizeof(buf),buf,*cName(),"\\die3"), st_Effect,SOUND_TYPE_MONSTER_DYING);

		m_HeavyBreathSnd.create	(pSettings->r_string(section,"heavy_breath_snd"), st_Effect,SOUND_TYPE_MONSTER_INJURING);
		m_BloodSnd.create		(pSettings->r_string(section,"heavy_blood_snd"), st_Effect,SOUND_TYPE_MONSTER_INJURING);
	}

	cam_Set					(eacFirstEye);

	// sheduler
	shedule.t_min				= shedule.t_max = 1;

	// ��������� ��������� ��������
	m_fDispBase					= pSettings->r_float		(section,"disp_base"		 );
	m_fDispBase					= deg2rad(m_fDispBase);

	m_fDispAim					= pSettings->r_float		(section,"disp_aim"		 );
	m_fDispAim					= deg2rad(m_fDispAim);

	m_fDispVelFactor			= pSettings->r_float		(section,"disp_vel_factor"	 );
	m_fDispAccelFactor			= pSettings->r_float		(section,"disp_accel_factor" );
	m_fDispCrouchFactor			= pSettings->r_float		(section,"disp_crouch_factor");
	m_fDispCrouchNoAccelFactor	= pSettings->r_float		(section,"disp_crouch_no_acc_factor");

	m_bActorShadows = (psActorFlags.test(AF_ACTOR_BODY)) ? false : true;

	LPCSTR							default_outfit = READ_IF_EXISTS(pSettings,r_string,section,"default_outfit",0);
	SetDefaultVisualOutfit			(default_outfit);
	LPCSTR							default_outfit_legs = pSettings->r_string(section, "default_outfit_legs");
	if (m_bActorShadows)
		SetDefaultVisualOutfit_legs		(default_outfit);
	else
		SetDefaultVisualOutfit_legs		(default_outfit_legs);

	m_bCanBeDrawLegs 			= true;

	if (CanBeDrawLegs())
		m_bDrawLegs						= true;
	else
		m_bDrawLegs						= false;

	invincibility_fire_shield_1st	= READ_IF_EXISTS(pSettings,r_string,section,"Invincibility_Shield_1st",0);
	invincibility_fire_shield_3rd	= READ_IF_EXISTS(pSettings,r_string,section,"Invincibility_Shield_3rd",0);
//-----------------------------------------
	m_AutoPickUp_AABB				= READ_IF_EXISTS(pSettings,r_fvector3,section,"AutoPickUp_AABB",Fvector().set(0.02f, 0.02f, 0.02f));
	m_AutoPickUp_AABB_Offset		= READ_IF_EXISTS(pSettings,r_fvector3,section,"AutoPickUp_AABB_offs",Fvector().set(0, 0, 0));

	CStringTable string_table;
	m_sCharacterUseAction			= "character_use";
	m_sDeadCharacterUseAction		= "dead_character_use";
	m_sDeadCharacterUseOrDragAction	= "dead_character_use_or_drag";
	m_sCarCharacterUseAction		= "car_character_use";
	m_sInventoryItemUseAction		= "inventory_item_use";
	m_sInventoryBoxUseAction		= "inventory_box_use";
	m_sTurretCharacterUseAction		= "turret_character_use";
	//---------------------------------------------------------------------
	m_sHeadShotParticle	= READ_IF_EXISTS(pSettings,r_string,section,"HeadShotParticle",0);

}

void CActor::PHHit(float P,Fvector &dir, CObject *who,s16 element,Fvector p_in_object_space, float impulse, ALife::EHitType hit_type /* = ALife::eHitTypeWound */)
{
	m_pPhysics_support->in_Hit(P,dir,who,element,p_in_object_space,impulse,hit_type,!g_Alive());
}

struct playing_pred
{
	IC	bool	operator()			(ref_sound &s)
	{
		return	(NULL != s._feedback() );
	}
};

void	CActor::Hit							(SHit* pHDS)
{
	pHDS->aim_bullet = false;

	SHit HDS = *pHDS;
	if( HDS.hit_type<ALife::eHitTypeBurn || HDS.hit_type >= ALife::eHitTypeMax )
	{
		string256	err;
		xr_sprintf		(err, "Unknown/unregistered hit type [%d]", HDS.hit_type);
		R_ASSERT2	(0, err );
	
	}

	bool bPlaySound = true;
	if (!g_Alive()) bPlaySound = false;

	if(	!sndHit[HDS.hit_type].empty()&& (ALife::eHitTypeTelepatic != HDS.hit_type))
	{
		ref_sound& S = sndHit[HDS.hit_type][Random.randI(sndHit[HDS.hit_type].size())];
		bool b_snd_hit_playing = sndHit[HDS.hit_type].end() != std::find_if(sndHit[HDS.hit_type].begin(), sndHit[HDS.hit_type].end(), playing_pred());

		if(ALife::eHitTypeExplosion == HDS.hit_type)
		{
			if (this == Level().CurrentControlEntity())
			{
				S.set_volume(10.0f);
				if(!m_sndShockEffector){
					m_sndShockEffector = new SndShockEffector();
					m_sndShockEffector->Start(this, float(S._handle()->length_sec() * 1000), HDS.damage() );
				}
			}
			else
				bPlaySound = false;
		}
		if (bPlaySound && !b_snd_hit_playing) 
		{
			Fvector point		= Position();
			point.y				+= CameraHeight();
			S.play_at_pos		(this, point);
		};
	}

	
	//slow actor, only when he gets hit
	if(HDS.hit_type == ALife::eHitTypeWound || HDS.hit_type == ALife::eHitTypeStrike)
	{
		hit_slowmo				= HDS.damage();
		clamp					(hit_slowmo,0.0f,1.f);
	}
	else
		hit_slowmo = 0.f;
	//---------------------------------------------------------------
	if (Level().CurrentViewEntity() == this && HDS.hit_type == ALife::eHitTypeFireWound)
	{
		CObject* pLastHitter = Level().Objects.net_Find(m_iLastHitterID);
		CObject* pLastHittingWeapon = Level().Objects.net_Find(m_iLastHittingWeaponID);
		HitSector(pLastHitter, pLastHittingWeapon);
	};

	if ((mstate_real&mcSprint) && Level().CurrentControlEntity() == this && 
		HDS.hit_type != ALife::eHitTypeTelepatic &&
		HDS.hit_type != ALife::eHitTypeRadiation 
		)
	{
		mstate_wishful	&=~mcSprint;
	};

	HitMark			(HDS.damage(), HDS.dir, HDS.who, HDS.bone(), HDS.p_in_bone_space, HDS.impulse, HDS.hit_type);

	float hit_power	= HitArtefactsOnBelt(HDS.damage(), HDS.hit_type);
	hit_power		= HitBoosters(hit_power, HDS.hit_type);

	hit_power *= m_fImmunityCoef;

	if (GodMode())
		{
			HDS.power = 0.0f;
			inherited::Hit(&HDS);
			return;
		}
	else 
		{
			HDS.power = hit_power;
			HDS.add_wound = true;
			inherited::Hit(&HDS);
		};
}

void CActor::HitMark	(float P, 
						 Fvector dir,			
						 CObject* who, 
						 s16 element, 
						 Fvector position_in_bone_space, 
						 float impulse,  
						 ALife::EHitType hit_type)
{
	// hit marker
	if ( (hit_type==ALife::eHitTypeFireWound||hit_type==ALife::eHitTypeWound_2) && g_Alive() && Local() && (Level().CurrentEntity()==this) )	
	{
		HUD().Hit(0, P, dir);

		CEffectorCam* ce = Cameras().GetCamEffector((ECamEffectorType)effFireHit);
		if(!ce)
		{
			int id						= -1;
			Fvector						cam_pos,cam_dir,cam_norm;
			cam_Active()->Get			(cam_pos,cam_dir,cam_norm);
			cam_dir.normalize_safe		();
			dir.normalize_safe			();

			float ang_diff				= angle_difference	(cam_dir.getH(), dir.getH());
			Fvector						cp;
			cp.crossproduct				(cam_dir,dir);
			bool bUp					= (cp.y>0.0f);

			Fvector cross;
			cross.crossproduct			(cam_dir, dir);
			VERIFY						(ang_diff>=0.0f && ang_diff<=PI);

			float _s1 = PI_DIV_8;
			float _s2 = _s1+PI_DIV_4;
			float _s3 = _s2+PI_DIV_4;
			float _s4 = _s3+PI_DIV_4;

			if(ang_diff<=_s1)
				id = 2;
			else if(ang_diff>_s1 && ang_diff<=_s2)
				id = (bUp)?5:7;
			else if(ang_diff>_s2 && ang_diff<=_s3)
				id = (bUp)?3:1;
			else if(ang_diff>_s3 && ang_diff<=_s4)
				id = (bUp)?4:6;
			else if(ang_diff>_s4)
				id = 0;
			else
				VERIFY(0);

			string64 sect_name;
			xr_sprintf(sect_name,"effector_fire_hit_%d",id);
			AddEffector(this, effFireHit, sect_name, P/100.0f);
		}
	}

	if (who && !fis_zero(P))
		callback(GameObject::eHit)(
			lua_game_object(), 
			P,
			dir,
			smart_cast<const CGameObject*>(who)->lua_game_object(), 
			element
		);

}

void CActor::HitSignal(float perc, Fvector& vLocalDir, CObject* who, s16 element)
{
	if (g_Alive()) 
	{

		// stop-motion
		if (character_physics_support()->movement()->Environment()==CPHMovementControl::peOnGround || character_physics_support()->movement()->Environment()==CPHMovementControl::peAtWall)
		{
			Fvector zeroV;
			zeroV.set			(0,0,0);
			character_physics_support()->movement()->SetVelocity(zeroV);
		}
		
		// check damage bone
		Fvector D;
		XFORM().transform_dir(D,vLocalDir);

		float	yaw, pitch;
		D.getHP(yaw,pitch);
		IKinematics *K = smart_cast<IKinematics*>(Visual());
		IKinematicsAnimated *KA = smart_cast<IKinematicsAnimated*>(Visual());
		VERIFY(K && KA);
#pragma todo("Dima to Dima : forward-back bone impulse direction has been determined incorrectly!")
		MotionID motion_ID = m_anims->m_normal.m_damage[iFloor(K->LL_GetBoneInstance(element).get_param(1) + (angle_difference(r_model_yaw + r_model_yaw_delta,yaw) <= PI_DIV_2 ? 0 : 1))];
		float power_factor = perc/100.f; clamp(power_factor,0.f,1.f);
		VERIFY(motion_ID.valid());
		KA->PlayFX(motion_ID,power_factor);
	}
}
void start_tutorial(LPCSTR name);
void CActor::Die(CObject* who)
{
	inherited::Die		(who);

	if (OnServer())
	{	
		xr_vector<CInventorySlot>::iterator I = inventory().m_slots.begin();
		xr_vector<CInventorySlot>::iterator E = inventory().m_slots.end();


		for (u32 slot_idx=0 ; I != E; ++I,++slot_idx)
		{
			if (slot_idx == inventory().GetActiveSlot()) 
			{
				if((*I).m_pIItem)
				{

					(*I).m_pIItem->SetDropManual(TRUE);

				};
			continue;
			}
			else
			{
				CCustomOutfit *pOutfit = smart_cast<CCustomOutfit *> ((*I).m_pIItem);
				if (pOutfit) continue;
			};
			if((*I).m_pIItem) 
				inventory().Ruck((*I).m_pIItem);
		};


		///!!! ������ �����
		TIItemContainer &l_blist = inventory().m_belt;
		while (!l_blist.empty())	
			inventory().Ruck(l_blist.front());

	};

	if (psActorFlags.test(AF_FST_PSN_DEATH))
	{
		cam_Set					(eacFirstEye);
		m_bActorShadows 			= true;
	} else
		cam_Set					(eacFreeLook);

	mstate_wishful	&=		~mcAnyMove;
	mstate_real		&=		~mcAnyMove;

	::Sound->play_at_pos	(sndDie[Random.randI(SND_DIE_COUNT)],this,Position());

	m_HeavyBreathSnd.stop	();
	m_BloodSnd.stop			();		
	

	start_tutorial		("game_over");

	CurrentGameUI()->HideShownDialogs();

	xr_delete				(m_sndShockEffector);
	xr_delete				(m_ScriptCameraDirection);
}

void	CActor::SwitchOutBorder(bool new_border_state)
{
	if(new_border_state)
	{
		callback(GameObject::eExitLevelBorder)(lua_game_object());
	}
	else 
	{
//.		Msg("enter level border");
		callback(GameObject::eEnterLevelBorder)(lua_game_object());
	}
	m_bOutBorder=new_border_state;
}

void CActor::g_Physics(Fvector& _accel, float jump, float dt)
{
	// Correct accel
	Fvector						accel;
	accel.set					(_accel);
	hit_slowmo					-=	dt;
	if (hit_slowmo<0)			hit_slowmo = 0.f;

	accel.mul					(1.f-hit_slowmo);
	
	if(g_Alive())
	{
	if(mstate_real&mcClimb&&!cameras[eacFirstEye]->bClampYaw)accel.set(0.f,0.f,0.f);
	character_physics_support()->movement()->Calculate			(accel,cameras[cam_active]->vDirection,0,jump,dt,false);
	bool new_border_state=character_physics_support()->movement()->isOutBorder();
	if(m_bOutBorder!=new_border_state && Level().CurrentControlEntity() == this)
	{
		SwitchOutBorder(new_border_state);
	}
	character_physics_support()->movement()->GetPosition		(Position());
	character_physics_support()->movement()->bSleep				=false;
	}

	if (Local() && g_Alive()) {
		if (character_physics_support()->movement()->gcontact_Was)
			Cameras().AddCamEffector		(new CEffectorFall(character_physics_support()->movement()->gcontact_Power));
		if (!fis_zero(character_physics_support()->movement()->gcontact_HealthLost))	{
			const ICollisionDamageInfo* di=character_physics_support()->movement()->CollisionDamageInfo();
			Fvector hdir;di->HitDir(hdir);
			SetHitInfo(this, NULL, 0, Fvector().set(0, 0, 0), hdir);
			//				Hit	(m_PhysicMovementControl->gcontact_HealthLost,hdir,di->DamageInitiator(),m_PhysicMovementControl->ContactBone(),di->HitPos(),0.f,ALife::eHitTypeStrike);//s16(6 + 2*::Random.randI(0,2))
			if (Level().CurrentControlEntity() == this)
			{
				SHit HDS = SHit(character_physics_support()->movement()->gcontact_HealthLost,hdir,di->DamageInitiator(),character_physics_support()->movement()->ContactBone(),di->HitPos(),0.f,di->HitType());
//				Hit(&HDS);

				NET_Packet	l_P;
				HDS.GenHeader(GE_HIT, ID());
				HDS.whoID = di->DamageInitiator()->ID();
				HDS.weaponID = di->DamageInitiator()->ID();
				HDS.Write_Packet(l_P);

				u_EventSend	(l_P);
			}
		}
	}
}
float g_fov;

float CActor::currentFOV()
{
	CWeapon* pWeapon = smart_cast<CWeapon*>(inventory().ActiveItem());	

	if (eacFreeLook != cam_active && pWeapon &&
		pWeapon->IsZoomed() && (!pWeapon->ZoomTexture() ||
		(!pWeapon->IsRotatingToZoom() && pWeapon->ZoomTexture() && eacFirstEye == cam_active) || (pWeapon->ZoomTexture() && eacLookAt == cam_active)))
		return pWeapon->GetZoomFactor() * (0.75f);
	else
		return g_fov;
}

extern u32 crosshairAnimationType;
void CActor::UpdateCL()
{
	if (m_feel_touch_characters > 0)
	{
		for (xr_vector<CObject*>::iterator it = feel_touch.begin(); it != feel_touch.end(); it++)
		{
			CPhysicsShellHolder	*sh = smart_cast<CPhysicsShellHolder*>(*it);
			if (sh&&sh->character_physics_support())
			{
				sh->character_physics_support()->movement()->UpdateObjectBox(character_physics_support()->movement()->PHCharacter());
			}
		}
	}
	if (m_holder)
		m_holder->UpdateEx(currentFOV());

	m_snd_noise -= 0.3f*Device.fTimeDelta;

	VERIFY2(_valid(renderable.xform), *cName());
	inherited::UpdateCL();
	VERIFY2(_valid(renderable.xform), *cName());
	m_pPhysics_support->in_UpdateCL();
	VERIFY2(_valid(renderable.xform), *cName());

	if (g_Alive()) {
		PickupModeUpdate();
		PickupModeUpdate_COD();
	}

	m_bZoomAimingMode = false;
	CWeapon* pWeapon = smart_cast<CWeapon*>(inventory().ActiveItem());	

	Device.Statistic->TEST1.Begin		();
	cam_Update(float(Device.dwTimeDelta)/1000.0f, currentFOV());
	Device.Statistic->TEST1.End		();

	if(Level().CurrentEntity() && this->ID()==Level().CurrentEntity()->ID() )
	{
		psHUD_Flags.set( HUD_CROSSHAIR_RT2, true );
		psHUD_Flags.set( HUD_DRAW_RT, true );
	}
	if(pWeapon )
	{
		if(pWeapon->IsZoomed())
		{
			CEffectorZoomInertion* S = smart_cast<CEffectorZoomInertion*>	(Cameras().GetCamEffector(eCEZoom));
			if (S)
			{
				S->SetParams(GetWeaponAccuracy() * pWeapon->GetZoomInertion() * m_fZoomInertCoef);
			}

			m_bZoomAimingMode = true;
		}

		if(Level().CurrentEntity() && this->ID()==Level().CurrentEntity()->ID() )
		{
			float fire_disp_full = pWeapon->GetFireDispersion(true) + GetWeaponAccuracy();

			HUD().SetCrosshairDisp(fire_disp_full, 0.02f);
			if (crosshairAnimationType != 0)
				HUD().ShowCrosshair(pWeapon->use_crosshair());
			else
				HUD().ShowCrosshair(false);

			if (eacLookAt == cam_active) {
				psHUD_Flags.set( HUD_CROSSHAIR_RT2, true );
				psHUD_Flags.set( HUD_DRAW_RT,		true );
			} else {
				psHUD_Flags.set( HUD_CROSSHAIR_RT2, pWeapon->show_crosshair() );
				psHUD_Flags.set( HUD_DRAW_RT,		pWeapon->show_indicators() ); 
			}
		}

	}
	else if (m_holder && smart_cast<CMountedTurret*>(m_holder))
	{
		HUD().SetCrosshairDisp(0.f);
		HUD().ShowCrosshair(true);
		psHUD_Flags.set(HUD_CROSSHAIR_RT2, true);
		psHUD_Flags.set(HUD_DRAW_RT, true);
	}
	else
	{
		if(Level().CurrentEntity() && this->ID()==Level().CurrentEntity()->ID() )
		{
			HUD().SetCrosshairDisp(0.f);
			HUD().ShowCrosshair(false);
		}
	}

	UpdateDefferedMessages();

	if (g_Alive()) 
		CStepManager::update();

	spatial.type |=STYPE_REACTTOSOUND;

	if(m_sndShockEffector)
	{
		if (this == Level().CurrentViewEntity())
		{
			m_sndShockEffector->Update();

			if(!m_sndShockEffector->InWork() || !g_Alive())
				xr_delete(m_sndShockEffector);
		}
		else
			xr_delete(m_sndShockEffector);
	}
	if (m_ScriptCameraDirection)
	{
		if (this == Level().CurrentViewEntity())
		{
			m_ScriptCameraDirection->Update();

			if(!m_ScriptCameraDirection->InWork() || !g_Alive())
				xr_delete(m_ScriptCameraDirection);
		}
		else
			xr_delete(m_ScriptCameraDirection);
	}

	CTorch *flashlight = GetCurrentTorch();
	if (flashlight)
		flashlight->UpdateBattery();

	Fmatrix							trans;
	if(cam_Active() == cam_FirstEye())
		Cameras().hud_camera_Matrix		(trans);
	else
		Cameras().camera_Matrix			(trans);

	if(IsFocused())
		g_player_hud->update			(trans);
}

float	NET_Jump = 0;

#include "ai\monsters\ai_monster_utils.h"
void CActor::shedule_Update	(u32 DT)
{
	setSVU(OnServer());

	if(IsFocused())
	{
		BOOL bHudView				= HUDview();
		if(bHudView)
		{
			CInventoryItem* pInvItem	= inventory().ActiveItem();	
			if( pInvItem )
			{
				CHudItem* pHudItem		= smart_cast<CHudItem*>(pInvItem);	
				if(pHudItem)
				{
					if( pHudItem->IsHidden() )
					{
						g_player_hud->detach_item	(pHudItem);
					}
					else
					{
						g_player_hud->attach_item	(pHudItem);
					}
				}
			}else
			{
					g_player_hud->detach_item_idx	( 0 );
					//Msg("---No active item in inventory(), item 0 detached.");
			}
		}
		else
		{
			g_player_hud->detach_all_items();
			//Msg("---No hud view found, all items detached.");
		}
			
	}

	//���������� ���������
	UpdateInventoryOwner			(DT);

	GameTaskManager().UpdateTasks	();

	if(m_holder || !getEnabled() || !Ready())
	{
		m_sDefaultObjAction				= NULL;
		inherited::shedule_Update		(DT);
		return;
	}

	// 
	clamp					(DT,0u,100u);
	float	dt	 			=  float(DT)/1000.f;

	// Check controls, create accel, prelimitary setup "mstate_real"
	
	if (Level().CurrentControlEntity() == this && (!Level().IsDemoPlay() || Level().IsServerDemo()))
	//------------------------------------------------
	{
		g_cl_CheckControls		(mstate_wishful,NET_SavedAccel,NET_Jump,dt);
		{

		}
		g_cl_Orientate			(mstate_real,dt);
		g_Orientate				(mstate_real,dt);

		g_Physics				(NET_SavedAccel,NET_Jump,dt);
		
		g_cl_ValidateMState		(dt,mstate_wishful);
		g_SetAnimation			(mstate_real);
		
		// Check for game-contacts
		Fvector C; float R;		
		//m_PhysicMovementControl->GetBoundingSphere	(C,R);
		
		Center(C);
		R=Radius();
		feel_touch_update		(C,R);

		// Dropping
		if (b_DropActivated)	{
			f_DropPower			+= dt*0.1f;
			clamp				(f_DropPower,0.f,1.f);
		} else {
			f_DropPower			= 0.f;
		}
		if (!Level().IsDemoPlay())
		{		
		//-----------------------------------------------------
		mstate_wishful &=~mcAccel;
		mstate_wishful &=~mcLStrafe;
		mstate_wishful &=~mcRStrafe;
		mstate_wishful &=~mcLLookout;
		mstate_wishful &=~mcRLookout;
		mstate_wishful &=~mcFwd;
		mstate_wishful &=~mcBack;
		extern bool g_bAutoClearCrouch;
		extern u32  g_bAutoApplySprint;
		if (g_bAutoClearCrouch){
			mstate_wishful &= ~mcCrouch;
			if (g_bAutoApplySprint > 0){
				g_bAutoApplySprint += 1;
			}
		}
		if (g_bAutoApplySprint == 10)//�������� ��� �� 10� ����
		{
			mstate_wishful |= mcSprint;
			g_bAutoApplySprint = 0;
		}
		//-----------------------------------------------------
		}
	}
	else 
	{
		make_Interpolation();
	
		if (NET.size())
		{
			
			g_sv_Orientate				(mstate_real,dt			);
			g_Orientate					(mstate_real,dt			);
			g_Physics					(NET_SavedAccel,NET_Jump,dt	);			
			if (!m_bInInterpolation)
				g_cl_ValidateMState			(dt,mstate_wishful);
			g_SetAnimation				(mstate_real);

			if (NET_Last.mstate & mcCrouch)
			{
				if (isActorAccelerated(mstate_real, IsZoomAimingMode()))
					character_physics_support()->movement()->ActivateBox(1, true);
				else
					character_physics_support()->movement()->ActivateBox(2, true);
			}
			else 
				character_physics_support()->movement()->ActivateBox(0, true);
		}	
		mstate_old = mstate_real;
	}

	NET_Jump = 0;

	inherited::shedule_Update	(DT);

	//�������� ���������� ��� ������
	if (!pCamBobbing)
	{
		pCamBobbing = new CEffectorBobbing();
		Cameras().AddCamEffector			(pCamBobbing);
	}
	pCamBobbing->SetState						(mstate_real, conditions().IsLimping(), IsZoomAimingMode());

	//���� �������� ������� ��� �������� � ��������
	if(this==Level().CurrentControlEntity())
	{
		if(conditions().IsLimping() && g_Alive())
		{
			if(!m_HeavyBreathSnd._feedback())
			{
				m_HeavyBreathSnd.play_at_pos(this, Fvector().set(0,ACTOR_HEIGHT,0), sm_Looped | sm_2D);
			}else{
				m_HeavyBreathSnd.set_position(Fvector().set(0,ACTOR_HEIGHT,0));
			}
		}else if(m_HeavyBreathSnd._feedback()){
			m_HeavyBreathSnd.stop		();
		}

		float bs = conditions().BleedingSpeed();
		if(bs>0.6f)
		{
			Fvector snd_pos;
			snd_pos.set(0,ACTOR_HEIGHT,0);
			if(!m_BloodSnd._feedback())
				m_BloodSnd.play_at_pos(this, snd_pos, sm_Looped | sm_2D);
			else
				m_BloodSnd.set_position(snd_pos);

			float v = bs+0.25f;

			m_BloodSnd.set_volume	(v);
		}else{
			if(m_BloodSnd._feedback())
				m_BloodSnd.stop();
		}

		if(!g_Alive()&&m_BloodSnd._feedback())
				m_BloodSnd.stop();
	}
	
	if (((BOOL)m_bActorShadows == psActorFlags.test(AF_ACTOR_BODY)) && g_Alive() && !m_holder)
	{
		if (m_bActorShadows)
			SetDefaultVisualOutfit_legs		(pSettings->r_string(*cNameSect(),"default_outfit_legs"));
		else
			SetDefaultVisualOutfit_legs		(GetDefaultVisualOutfit());

		m_bActorShadows = (psActorFlags.test(AF_ACTOR_BODY)) ? false : true;

		if (eacFirstEye == cam_active) //reset visual
		{
			cam_Set(eacLookAt);
			cam_Set(eacFirstEye);
		}
	}

	//���� � ������ HUD, �� ���� ������ ������ �� ��������
	if(!character_physics_support()->IsRemoved())
		if (m_bDrawLegs && ((!psDeviceFlags.test(rsR2) && !psDeviceFlags.test(rsR3) && !psDeviceFlags.test(rsR4) && !m_bActorShadows) || ((psDeviceFlags.test(rsR2) || psDeviceFlags.test(rsR3) || psDeviceFlags.test(rsR4)) && m_bActorShadows)))
			setVisible				(TRUE);
		else
			setVisible				(!HUDview	());
	//��� ����� ����� ����� �����
	collide::rq_result& RQ = HUD().GetCurrentRayQuery();
	
	float dist_to_obj = RQ.range;
	if (RQ.O && eacFirstEye != cam_active)
		dist_to_obj = get_bone_position(this, "bip01_spine").distance_to((smart_cast<CGameObject*>(RQ.O))->Position());

	if(!input_external_handler_installed() && RQ.O && dist_to_obj<inventory().GetTakeDist()) 
	{
		m_pObjectWeLookingAt			= smart_cast<CGameObject*>(RQ.O);
		
		CGameObject						*game_object = smart_cast<CGameObject*>(RQ.O);
		m_pUsableObject					= smart_cast<CUsableScriptObject*>(game_object);
		m_pInvBoxWeLookingAt			= smart_cast<CInventoryBox*>(game_object);
		inventory().m_pTarget			= smart_cast<PIItem>(game_object);
		m_pPersonWeLookingAt			= smart_cast<CInventoryOwner*>(game_object);
		m_pHolderWeLookingAt			= smart_cast<CHolderCustom*>(game_object);
		CEntityAlive* pEntityAlive		= smart_cast<CEntityAlive*>(game_object);
		
			if (m_pUsableObject && m_pUsableObject->tip_text())
			{
				m_sDefaultObjAction = CStringTable().translate( m_pUsableObject->tip_text() );
			}
			else
			{
				if (m_pPersonWeLookingAt && pEntityAlive && pEntityAlive->g_Alive())
					m_sDefaultObjAction = m_sCharacterUseAction;

				else if (pEntityAlive && !pEntityAlive->g_Alive())
				{
					bool b_allow_drag = !!pSettings->line_exist("ph_capture_visuals",pEntityAlive->cNameVisual());

					if (m_pPersonWeLookingAt && !m_pPersonWeLookingAt->inventory().CanBeDragged())
						b_allow_drag = false;

					if(b_allow_drag)
						m_sDefaultObjAction = m_sDeadCharacterUseOrDragAction;
					else
						m_sDefaultObjAction = m_sDeadCharacterUseAction;

				}
				else if (m_pHolderWeLookingAt)
				{
					if (smart_cast<CCar*>(m_pHolderWeLookingAt))
						m_sDefaultObjAction = m_sCarCharacterUseAction;
					else
						m_sDefaultObjAction = m_sTurretCharacterUseAction;
				}

				else if (inventory().m_pTarget && inventory().m_pTarget->CanTake() )
					m_sDefaultObjAction = m_sInventoryItemUseAction;

				else 
					m_sDefaultObjAction = NULL;
			}
	}
	else 
	{
		inventory().m_pTarget	= NULL;
		m_pPersonWeLookingAt	= NULL;
		m_sDefaultObjAction		= NULL;
		m_pUsableObject			= NULL;
		m_pObjectWeLookingAt	= NULL;
		m_pHolderWeLookingAt	= NULL;
		m_pInvBoxWeLookingAt	= NULL;
	}

	//��� ������ ����������, ����������� �� �����
	UpdateArtefactsOnBelt						();
	m_pPhysics_support->in_shedule_Update		(DT);
	Check_for_AutoPickUp						();
};

void CActor::renderable_Render	()
{
	inherited::renderable_Render			();
	if (!HUDview() || (m_bActorShadows && m_bFirstEye)){
		CInventoryOwner::renderable_Render	();
	}
}

BOOL CActor::renderable_ShadowGenerate	() 
{
	if(m_holder || (!m_bActorShadows && m_bFirstEye))
		return FALSE;
	
	return inherited::renderable_ShadowGenerate();
}

void CActor::g_PerformDrop	( )
{
	b_DropActivated			= FALSE;

	PIItem pItem			= inventory().ActiveItem();
	if (0==pItem)			return;

	u32 s					= inventory().GetActiveSlot();
	if(inventory().m_slots[s].m_bPersistent)	return;

	pItem->SetDropManual	(TRUE);
}


// HUD
void CActor::OnHUDDraw	(CCustomHUD* /**hud/**/)
{
	if (IsFocused() && !((mstate_real & mcLookout))){
		g_player_hud->render_hud();
	}
}

void CActor::SetPhPosition(const Fmatrix &transform)
{
	if(!m_pPhysicsShell){ 
		character_physics_support()->movement()->SetPosition(transform.c);
	}
}

void CActor::ForceTransform(const Fmatrix& m)
{
	if(!g_Alive())				return;
	XFORM().set					(m);
	if(character_physics_support()->movement()->CharacterExist()) character_physics_support()->movement()->EnableCharacter	();
	character_physics_support()->set_movement_position( m.c );
	character_physics_support()->movement()->SetVelocity		(0,0,0);
}

ENGINE_API extern float		psHUD_FOV;
float CActor::Radius()const
{ 
	float R		= inherited::Radius();
	CWeapon* W	= smart_cast<CWeapon*>(inventory().ActiveItem());
	if (W) R	+= W->Radius();
	//	if (HUDview()) R *= 1.f/psHUD_FOV;
	return R;
}

bool		CActor::use_bolts				() const
{
	return CInventoryOwner::use_bolts();
};

int		g_iCorpseRemove = 1;

bool  CActor::NeedToDestroyObject() const
{
	if(g_Alive())				return false;
	if(g_iCorpseRemove == -1)	return false;
	if(g_iCorpseRemove == 0 && m_bAllowDeathRemove) return true;
	return (TimePassedAfterDeath()>m_dwBodyRemoveTime && m_bAllowDeathRemove);
}

ALife::_TIME_ID	 CActor::TimePassedAfterDeath()	const
{
	if(!g_Alive())
		return Level().timeServer() - GetLevelDeathTime();
	else
		return 0;
}


void CActor::OnItemTake			(CInventoryItem *inventory_item)
{
	CInventoryOwner::OnItemTake(inventory_item);
}

void CActor::OnItemDrop			(CInventoryItem *inventory_item)
{
	//change of actor hud and visual if there is no outfit in slot and droped item.type is outfit
	PIItem	outfit = this->inventory().ItemFromSlot(OUTFIT_SLOT);
	CCustomOutfit* pOutfit	= smart_cast<CCustomOutfit*>	(inventory_item);

	if (!outfit && pOutfit)
	{
		if (this->IsFirstEye())
		{
			shared_str DefVisual = this->GetDefaultVisualOutfit_legs();
			if (DefVisual.size())
			{
				this->ChangeVisual(DefVisual);
			}
		} else {
			shared_str DefVisual = this->GetDefaultVisualOutfit();
			if (DefVisual.size())
			{
				this->ChangeVisual(DefVisual);
			}
		}

		if (this == Level().CurrentViewEntity())
			g_player_hud->load_default();
	}


	CWeapon* weapon = smart_cast<CWeapon*>(inventory_item);
	if (weapon && weapon->m_eItemPlace == eItemPlaceSlot)
	{
		weapon->OnZoomOut();
		if (weapon->GetRememberActorNVisnStatus())
			weapon->EnableActorNVisnAfterZoom();
	}

	CInventoryOwner::OnItemDrop(inventory_item);
}


void CActor::OnItemDropUpdate ()
{
	CInventoryOwner::OnItemDropUpdate		();

	TIItemContainer::iterator				I = inventory().m_all.begin();
	TIItemContainer::iterator				E = inventory().m_all.end();
	
	for ( ; I != E; ++I)
		if( !(*I)->IsInvalid() && !attached(*I))
			attach(*I);
}


void CActor::OnItemRuck		(CInventoryItem *inventory_item, EItemPlace previous_place)
{
	CInventoryOwner::OnItemRuck(inventory_item, previous_place);

}
void CActor::OnItemBelt		(CInventoryItem *inventory_item, EItemPlace previous_place)
{
	CInventoryOwner::OnItemBelt(inventory_item, previous_place);

}

#define ARTEFACTS_UPDATE_TIME 0.100f

void CActor::UpdateArtefactsOnBelt()
{
	static float update_time = 0;

	float f_update_time = 0;

	if(update_time<ARTEFACTS_UPDATE_TIME)
	{
		update_time += conditions().fdelta_time();
		return;
	}
	else
	{
		f_update_time	= update_time;
		update_time		= 0.0f;
	}

	//tatarinrafa: added additional jump speed sprint speed walk speed
	float run_koef_additional		= 0.0f;
	float sprint_koef_additional	= 0.0f;
	float jump_koef_additional		= 0.0f;

	for(TIItemContainer::iterator it = inventory().m_belt.begin(); 
		inventory().m_belt.end() != it; ++it) 
	{
		CArtefact*	artefact = smart_cast<CArtefact*>(*it);
		if(artefact)
		{
			conditions().ChangeBleeding			(artefact->m_fBleedingRestoreSpeed*f_update_time);
			conditions().ChangeHealth			(artefact->m_fHealthRestoreSpeed*f_update_time);
			conditions().ChangePower			(artefact->m_fPowerRestoreSpeed*f_update_time);
			conditions().ChangeSatiety			(artefact->m_fSatietyRestoreSpeed*f_update_time);
			conditions().ChangeRadiation		(artefact->m_fRadiationRestoreSpeed*f_update_time);
			conditions().ChangePsyHealth		(artefact->m_fPsyhealthRestoreSpeed*f_update_time);

			//������ ������ �������� �� ���������� �� �����
			run_koef_additional		 += artefact->m_additional_run_coef;
			sprint_koef_additional	 += artefact->m_additional_sprint_koef;
			jump_koef_additional	 += artefact->m_additional_jump_speed;
		}
	}

	//�������� �� ��������� �� ����� �������� � ����� ���. ������ ��� �����. ��� �������� �� ���������
	if (run_koef_additional > m_fRunFactorAdditionalLimit)
	run_koef_additional = m_fRunFactorAdditionalLimit;
	if (sprint_koef_additional >m_fSprintFactorAdditionalLimit)
	sprint_koef_additional = m_fSprintFactorAdditionalLimit;
	if (jump_koef_additional >m_fJumpFactorAdditionalLimit)
	jump_koef_additional = m_fJumpFactorAdditionalLimit;
	
	//������ ���������� ���� ���������� � ������� ����� ��
	CInventoryItem* itemfromoutfitslot = inventory().ItemFromSlot(OUTFIT_SLOT);
	if (itemfromoutfitslot){
		CCustomOutfit* outfit = smart_cast<CCustomOutfit*>(itemfromoutfitslot);
		if (outfit){
			//������� ������ �� �������
			run_koef_additional		+= outfit->m_additional_run_coef;
			sprint_koef_additional	+= outfit->m_additional_sprint_koef;
			jump_koef_additional	+= outfit->m_additional_jump_speed;
		}
	}

	m_fSprintFactorAdditional	= sprint_koef_additional;
	m_fRunFactorAdditional		= run_koef_additional;
	character_physics_support()->movement()->SetJumpUpVelocity(m_fJumpSpeed + jump_koef_additional);//��� ������ ������� ���������
}

float	CActor::HitArtefactsOnBelt		(float hit_power, ALife::EHitType hit_type)
{
	float res_hit_power_k		= 1.0f;
	float _af_count				= 0.0f;
	for(TIItemContainer::iterator it = inventory().m_belt.begin(); 
		inventory().m_belt.end() != it; ++it) 
	{
		CArtefact*	artefact = smart_cast<CArtefact*>(*it);
		if(artefact){
			res_hit_power_k	+= artefact->m_ArtefactHitImmunities.AffectHit(1.0f, hit_type);
			_af_count		+= 1.0f;
		}
	}
	res_hit_power_k			-= _af_count;

	return					res_hit_power_k * hit_power;
}


float	CActor::HitBoosters(float hit_power, ALife::EHitType hit_type)
{
	float res_hit_power_k = 1.0f;
	float _af_count = 0.0f;

	// tatarinrafa: booster stuff
	for (int i = 0; i < conditions().Effectors.size(); i++){
		if (conditions().Effectors[i].bIsBooster && (conditions().Effectors[i].fEffectorDuration >= Device.fTimeGlobal) && (conditions().Effectors[i].fEffectorUseTime < Device.fTimeGlobal)){
			res_hit_power_k += conditions().Effectors[i].m_EffectHitImmunities.AffectHit(1.0f, hit_type);
			_af_count += 1.0f;
		}
	}
	res_hit_power_k -= _af_count;
	return					res_hit_power_k * hit_power;
}


void	CActor::SetZoomRndSeed		(s32 Seed)
{
	if (0 != Seed) m_ZoomRndSeed = Seed;
	else m_ZoomRndSeed = s32(Level().timeServer_Async());
};

void	CActor::SetShotRndSeed		(s32 Seed)
{
	if (0 != Seed) m_ShotRndSeed = Seed;
	else m_ShotRndSeed = s32(Level().timeServer_Async());
};

Fvector CActor::GetMissileOffset	() const
{
	return m_vMissileOffset;
}

void CActor::SetMissileOffset		(const Fvector &vNewOffset)
{
	m_vMissileOffset.set(vNewOffset);
}

void CActor::spawn_supplies			()
{
	inherited::spawn_supplies		();
	CInventoryOwner::spawn_supplies	();
}


void CActor::AnimTorsoPlayCallBack(CBlend* B)
{
	CActor* actor		= (CActor*)B->CallbackParam;
	actor->m_bAnimTorsoPlayed = FALSE;
}

void CActor::SetActorVisibility(u16 who, float value)
{
	CUIMotionIcon		&motion_icon	= CurrentGameUI()->UIMainIngameWnd->MotionIcon();
	motion_icon.SetActorVisibility		(who, value);
}

CPHDestroyable*	CActor::ph_destroyable	()
{
	return smart_cast<CPHDestroyable*>(character_physics_support());
}

CEntityConditionSimple *CActor::create_entity_condition	(CEntityConditionSimple* ec)
{
	if(!ec)
		m_entity_condition		= new CActorCondition(this);
	else
		m_entity_condition		= smart_cast<CActorCondition*>(ec);
	
	return		(inherited::create_entity_condition(m_entity_condition));
}

DLL_Pure *CActor::_construct			()
{
	m_pPhysics_support				= new CCharacterPhysicsSupport(CCharacterPhysicsSupport::etActor,this);
	CEntityAlive::_construct		();
	CInventoryOwner::_construct		();
	CStepManager::_construct		();
	
	return							(this);
}

bool CActor::use_center_to_aim			() const
{
	return							(!(mstate_real&mcCrouch));
}



bool CActor::can_attach(const CInventoryItem *inventory_item) const
{
	const CAttachableItem	*item = smart_cast<const CAttachableItem*>(inventory_item);
	if (!item || (item && !item->can_be_attached()))/*(!item->enabled() || !item->can_be_attached()))*/
		return			(false);

	//����� �� ������������ ������� ������ ����
	if( m_attach_item_sections.end() == std::find(m_attach_item_sections.begin(),m_attach_item_sections.end(),inventory_item->object().cNameSect()) )
		return false;

	//���� ��� ���� �������������� ����� ������ ���� 
	if(attached(inventory_item->object().cNameSect()))
		return false;

	return true;
}

void CActor::OnDifficultyChanged	()
{
	// immunities
	VERIFY(g_SingleGameDifficulty>=egdNovice && g_SingleGameDifficulty<=egdMaster); 
	LPCSTR diff_name				= get_token_name(difficulty_type_token, g_SingleGameDifficulty);
	string128						tmp;
	strconcat						(sizeof(tmp),tmp,"actor_immunities_",diff_name);
	conditions().LoadImmunities		(tmp,pSettings);
	// hit probability
	strconcat						(sizeof(tmp),tmp,"hit_probability_",diff_name);
	hit_probability					= pSettings->r_float(*cNameSect(),tmp);
}

CVisualMemoryManager	*CActor::visual_memory	() const
{
	return							(&memory().visual());
}

float		CActor::GetMass				()
{
	return g_Alive()?character_physics_support()->movement()->GetMass():m_pPhysicsShell?m_pPhysicsShell->getMass():0; 
}

bool CActor::is_on_ground()
{
	return (character_physics_support()->movement()->Environment() != CPHMovementControl::peInAir);
}

CCustomOutfit* CActor::GetOutfit() const
{
	PIItem _of	= inventory().m_slots[OUTFIT_SLOT].m_pIItem;
	return _of?smart_cast<CCustomOutfit*>(_of):NULL;
}

void CActor::RechargeTorchBattery(void)
{
	m_current_torch->Recharge();
}

CTorch *CActor::GetCurrentTorch(void)
{
	if (inventory().ItemFromSlot(TORCH_SLOT))
	{
		CTorch *torch = smart_cast<CTorch*>(inventory().ItemFromSlot(TORCH_SLOT));
		if (torch)
			m_current_torch = torch;
		else
			m_current_torch = 0;
	} else
		m_current_torch = 0;

	return m_current_torch;
}

bool CActor::UsingTurret()
{
	return m_holder && smart_cast<CMountedTurret*>(m_holder);
}

u16 CActor::GetTurretTemp()
{
	CMountedTurret *turret = smart_cast<CMountedTurret*>(m_holder);
	R_ASSERT(turret);
	return turret->GetTemperature();
}

void CActor::SetDirectionSlowly(Fvector pos, float time)
{
	if(!m_ScriptCameraDirection)
		m_ScriptCameraDirection = new CScriptCameraDirection();

	m_ScriptCameraDirection->Start(this, pos, time);
}

void CActor::SetIconState(EActorState state, bool show)
{
	m_icons_state.set					(1 << state, show);
}

float CActor::SetWalkAccel(float new_value)
{
	float old_value = m_fWalkAccel;
	m_fWalkAccel = new_value;
	return old_value;
}
