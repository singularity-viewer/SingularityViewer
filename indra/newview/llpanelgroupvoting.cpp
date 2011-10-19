/** 
 * @file llpanelgroupvoting.cpp
 * @brief LLPanelGroupVoting class definition.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "roles_constants.h"

#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "llscrolllistctrl.h"
#include "llradiogroup.h"
#include "llspinctrl.h"
#include "llpanelgroupvoting.h"
#include "llnamelistctrl.h"
#include "llbutton.h"
#include "llnotify.h"

#include "llagent.h"
#include "llfocusmgr.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llviewerregion.h"

class LLPanelGroupVoting::impl
{
public:
	impl(LLPanelGroupVoting& panel, const LLUUID& group_id);
	~impl();

	static void onClickCreateProposal(void* userdata);
	static void onClickYes(void *userdata);
	static void onClickNo(void *userdata);
	static void onClickAbstain(void *userdata);
	static void onClickSubmitProposal(void *userdata);
	static void onClickCancelProposal(void *userdata);
	static void onClickViewProposalList(void *userdata);
	static void onClickViewProposalItem(void *userdata);
	static void onDoubleClickProposal(void *userdata);

	static void onDoubleClickHistoryItem(void *userdata);
	static void onClickViewHistoryItem(void *userdata);
	static void onClickViewHistoryList(void *userdata);

	void sendGroupProposalsRequest(const LLUUID& group_id);
	void sendStartGroupProposal();
	void sendGroupProposalBallot(const std::string& vote);
	void sendGroupVoteHistoryRequest(const LLUUID& group_id);

	void setEnableCreateProposal();
	void setEnableVoteProposal();
	void setEnableListProposals();
	void setEnableHistoryItem();
	void setEnableHistoryList();

	void updateQuorumText();
	void addPendingActiveScrollListItem(unsigned int current,
										unsigned int expected,
										EAddPosition pos);
	void addPendingHistoryScrollListItem(unsigned int current,
										 unsigned int expected,
										 EAddPosition pos);
	void addNoActiveScrollListItem(EAddPosition pos);
	void addNoHistoryScrollListItem(EAddPosition pos);
													 

	static void processGroupActiveProposalItemReply(LLMessageSystem* msg,
													void** data);
	static void processGroupVoteHistoryItemReply(LLMessageSystem* msg,
												 void** data);

public:
	LLUUID mGroupID;
	LLUUID mProposalID;

	LLPanelGroupVoting& mPanel;
	
	LLSpinCtrl		*mQuorum;
	LLTextBox		*mQuorumLbl;
	LLSpinCtrl		*mDuration;
	LLTextBox		*mDurationLbl;
	LLTextBox       *mDurationText;
	LLRadioGroup	*mMajority;
	LLTextBox		*mMajorityLbl;
	LLTextBox		*mStartLbl;
	LLTextBox	    *mEndLbl;
	LLTextBox	    *mStartDate;
	LLTextBox	    *mEndDate;

	LLButton *mBtnYes;
	LLButton *mBtnNo;
	LLButton *mBtnAbstain;

	LLScrollListCtrl *mProposals;
	LLTextEditor     *mProposalText;
	LLButton	     *mBtnCreateProposal;
	LLButton	     *mBtnSubmitProposal;
	LLButton         *mBtnCancelProposal;
	LLButton	     *mBtnViewProposalList;
	LLButton	     *mBtnViewProposalItem;

	LLScrollListCtrl *mVotesHistory;
	LLTextBox	     *mVotesHistoryLbl;
	LLTextEditor	 *mVoteHistoryText;
	LLTextBox        *mVoteHistoryTextLbl;
	LLButton		 *mBtnViewHistoryList;
	LLButton		 *mBtnViewHistoryItem;

	int	mNumGroupMembers;

	std::vector<LLSD> mActiveReceived;
	std::vector<LLSD> mHistoryReceived;
	U32 mOtherGroupHistoryItems;

	int mProposalColumnWidths[10];
	int mHistoryColumnWidths[10];

	LLUUID mProposalTransID;
	LLUUID mHistoryTransID;

	static std::map<LLUUID, LLPanelGroupVoting::impl*> sGroupIDs;
};

// ****************************************
// ** LLPanelGroupVoting::impl Functions **
// ****************************************

LLPanelGroupVoting::impl::impl(LLPanelGroupVoting& panel, const LLUUID& group_id)
	: mPanel(panel),
	  mGroupID(group_id)
{

	mNumGroupMembers = 0;

	mProposalTransID = LLUUID::null;
	mHistoryTransID  = LLUUID::null;

	mProposalColumnWidths[0] = -1;
	mProposalColumnWidths[1] = 210;
	mProposalColumnWidths[2] = 120;
	mProposalColumnWidths[3] = -1;
	mProposalColumnWidths[4] = 30;
	for (int index = 5; index < 10; index++)
	{
		mProposalColumnWidths[index] = -1;
	}

	mHistoryColumnWidths[0] = -1;
	mHistoryColumnWidths[1] = 210; // vote text sans non-printable
	mHistoryColumnWidths[2] = 120;
	mHistoryColumnWidths[3] = -1;
	mHistoryColumnWidths[4] = -1;
	mHistoryColumnWidths[5] = -1;	// full vote text

	//problem what if someone has both the group floater open and the finder
	//open to the same group?  Some maps that map group ids to panels
	//will then only be working for the last panel for a given group id :(
	sGroupIDs[group_id] = this;

	mQuorum         = NULL;
	mQuorumLbl      = NULL;
	mDuration       = NULL;
	mDurationLbl    = NULL;
	mDurationText   = NULL;
	mMajority       = NULL;
	mMajorityLbl    = NULL;
	mStartLbl       = NULL;
	mEndLbl         = NULL;
	mStartDate      = NULL;
	mEndDate        = NULL;

	mBtnYes     = NULL;
	mBtnNo      = NULL;
	mBtnAbstain = NULL;

	mProposals           = NULL;
	mProposalText        = NULL;
	mBtnCreateProposal   = NULL;
	mBtnSubmitProposal   = NULL;
	mBtnCancelProposal   = NULL;
	mBtnViewProposalList = NULL;
	mBtnViewProposalItem = NULL;

	mVotesHistory       = NULL;
	mVotesHistoryLbl    = NULL;
	mVoteHistoryText    = NULL;
	mVoteHistoryTextLbl = NULL;
	mBtnViewHistoryList = NULL;
	mBtnViewHistoryItem = NULL;

	mOtherGroupHistoryItems = 0;
}

LLPanelGroupVoting::impl::~impl()
{
	sGroupIDs.erase(mGroupID);
}

void LLPanelGroupVoting::impl::setEnableListProposals()
{
	//we make the assumption here that anyone who has the start
	//or vote powers should also have the view powers
	
	/* This power was removed to make group roles simpler
	if (!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_VIEW) &&
		!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_VOTE) &&
		!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_START) )
	*/
	if (!gAgent.isInGroup(mGroupID))
		return;


	mPanel.childSetText("proposal_header", mPanel.getString("proposals_header_view_txt"));
	mPanel.childSetText("proposal_instructions", mPanel.getString("proposals_header_view_inst"));
	mPanel.childSetVisible("proposal_lbl", FALSE);
	mPanel.childSetVisible("proposal_voting_lbl", TRUE);
	mProposals->setVisible(TRUE);
	mProposals->setBgSelectedColor(gColors.getColor("ScrollSelectedBGColor"));
	mProposalText->setVisible(FALSE);
	mBtnYes->setEnabled(FALSE);
	mBtnYes->setVisible(FALSE);
	mBtnNo->setEnabled(FALSE);
	mBtnNo->setVisible(FALSE);
	mBtnAbstain->setEnabled(FALSE);
	mBtnAbstain->setVisible(FALSE);
	mBtnSubmitProposal->setEnabled(FALSE);
	mBtnSubmitProposal->setVisible(FALSE);
	mBtnCancelProposal->setEnabled(FALSE);
	mBtnCancelProposal->setVisible(FALSE);

	mBtnViewProposalItem->setEnabled(TRUE);
	mBtnViewProposalItem->setVisible(TRUE);

	mBtnViewProposalList->setEnabled(FALSE);
	mBtnViewProposalList->setVisible(FALSE);

	mQuorumLbl->setVisible(FALSE);
	mQuorum->setEnabled(FALSE);
	mQuorum->setVisible(FALSE);
	mPanel.childSetVisible("quorum_text", FALSE);

	mDurationLbl->setVisible(FALSE);
	mDuration->setEnabled(FALSE);
	mDuration->setVisible(FALSE);
	mDurationText->setVisible(FALSE);

	mMajorityLbl->setVisible(FALSE);
	mMajority->setEnabled(FALSE);
	mMajority->setVisible(FALSE);
	mStartLbl->setVisible(FALSE);
	mStartDate->setVisible(FALSE);
	mEndLbl->setVisible(FALSE);
	mEndDate->setVisible(FALSE);

	//check to see if the agent can create proposals when enabling the
	//create proposal button
	BOOL can_create = gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_START);

	mBtnCreateProposal->setEnabled(can_create);
	mBtnCreateProposal->setVisible(can_create);
}

void LLPanelGroupVoting::impl::setEnableVoteProposal()
{
	if (!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_VOTE))
		return;

	LLScrollListItem *item = mProposals->getFirstSelected();
	if (item)
	{
		std::string already_voted;
		std::string vote_cast;

		mProposalID = item->getUUID();
		
		// col 0: index id
		LLScrollListCell * proposal_cell = item->getColumn(1);
		if ( proposal_cell )
		{
			 //proposal text
			mProposalText->setText(proposal_cell->getValue().asString());
		}
		else
		{	// Something's wrong... should have some text
			mProposalText->setText(LLStringUtil::null);
		}

		proposal_cell = item->getColumn(2);
		if (proposal_cell)
		{
			//end date
			mEndDate->setText(proposal_cell->getValue().asString());
		}
		else
		{	// Something's wrong... should have some text
			mEndDate->setText(LLStringUtil::null);
		}

		// col 3: Vote Type
		proposal_cell = item->getColumn(3);
		if (proposal_cell)
		{
			//already voted
			already_voted = proposal_cell->getValue().asString();
		}
		else
		{	// Something's wrong... should have some text
			already_voted = "";
		}

		proposal_cell = item->getColumn(5);
		if (proposal_cell)
		{
			//start date
			mStartDate->setText(proposal_cell->getValue().asString());
		}
		else
		{	// Something's wrong... should have some text
			mStartDate->setText(LLStringUtil::null);
		}

		proposal_cell = item->getColumn(6);
		if (proposal_cell)
		{
			// Vote Cast
			vote_cast = proposal_cell->getValue().asString();
		}

		// col 8: Vote Initiator
		proposal_cell = item->getColumn(8);
		if (proposal_cell)
		{
			//quorum
			mQuorum->set(
				(F32)atoi(proposal_cell->getValue().asString().c_str()));
		}
		else
		{
			mQuorum->set(0);
		}

		F32 majority = 0.0f;
		proposal_cell = item->getColumn(9);
		if (proposal_cell)
		{
			//majority
			majority =
				(F32)atof(proposal_cell->getValue().asString().c_str());
		}

		if(majority == 0.0f)
		{	// Select the Simple Majority
			mMajority->setSelectedIndex(0);
		}
		else if (majority == 1.0f)
		{
			//Select Unanimous
			mMajority->setSelectedIndex(2);
		}
		else
		{
			//Select 2/3 Majority
			mMajority->setSelectedIndex(1);
		}

		if (already_voted == "Yes")
		{
			LLUIString votestr = mPanel.getString("proposals_header_voted_inst");
			votestr.setArg("[VOTE]", vote_cast);
			mPanel.childSetText("proposal_instructions", votestr.getString());

			mBtnYes->setEnabled(FALSE);
			mBtnNo->setEnabled(FALSE);
			mBtnAbstain->setEnabled(FALSE);
		}
		else
		{
			mPanel.childSetText("proposal_instructions", mPanel.getString("proposals_header_vote_inst"));

			mBtnYes->setEnabled(TRUE);
			mBtnNo->setEnabled(TRUE);
			mBtnAbstain->setEnabled(TRUE);
		}

		mPanel.childSetText("proposal_header", mPanel.getString("proposals_header_vote_txt"));
		mPanel.childSetVisible("proposal_lbl", TRUE);
		mPanel.childSetVisible("proposal_voting_lbl", FALSE);
		mProposals->setVisible(FALSE);
		mProposalText->setEnabled(FALSE);
		mProposalText->setVisible(TRUE);
		mBtnYes->setVisible(TRUE);
		mBtnNo->setVisible(TRUE);
		mBtnAbstain->setVisible(TRUE);
		mBtnSubmitProposal->setEnabled(FALSE);
		mBtnSubmitProposal->setVisible(FALSE);
		mBtnCreateProposal->setEnabled(FALSE);
		mBtnCreateProposal->setVisible(FALSE);
		mBtnCancelProposal->setEnabled(FALSE);
		mBtnCancelProposal->setVisible(FALSE);

		mBtnViewProposalItem->setEnabled(FALSE);
		mBtnViewProposalItem->setVisible(FALSE);

		mBtnViewProposalList->setEnabled(TRUE);
		mBtnViewProposalList->setVisible(TRUE);

		mQuorumLbl->setVisible(TRUE);
		mQuorum->setEnabled(FALSE);
		mQuorum->setVisible(TRUE);
		mPanel.childSetVisible("quorum_text", TRUE);

		mDurationLbl->setVisible(FALSE);
		mDuration->setEnabled(FALSE);
		mDuration->setVisible(FALSE);
		mDurationText->setVisible(FALSE);

		mMajorityLbl->setVisible(TRUE);
		mMajority->setEnabled(FALSE);
		mMajority->setVisible(TRUE);
		mStartLbl->setVisible(TRUE);
		mStartDate->setVisible(TRUE);
		mEndLbl->setVisible(TRUE);
		mEndDate->setVisible(TRUE);
	}
}

void LLPanelGroupVoting::impl::setEnableCreateProposal()
{
	if ( !gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_START) )
		 return;

	mPanel.childSetText("proposal_header", mPanel.getString("proposals_header_create_txt"));
	mPanel.childSetText("proposal_instructions", mPanel.getString("proposals_header_create_inst"));
	mPanel.childSetVisible("proposal_lbl", TRUE);
	mPanel.childSetVisible("proposal_voting_lbl", FALSE);
	mProposals->setVisible(FALSE); 
	mProposalText->setEnabled(TRUE);
	mProposalText->setVisible(TRUE);
	mProposalText->setText(LLStringUtil::null);
	mBtnYes->setEnabled(FALSE);
	mBtnYes->setVisible(FALSE);
	mBtnNo->setEnabled(FALSE);
	mBtnNo->setVisible(FALSE);
	mBtnAbstain->setEnabled(FALSE);
	mBtnAbstain->setVisible(FALSE);
	mBtnSubmitProposal->setEnabled(TRUE);
	mBtnSubmitProposal->setVisible(TRUE);
	mBtnCancelProposal->setEnabled(TRUE);
	mBtnCancelProposal->setVisible(TRUE);
	mBtnCreateProposal->setEnabled(FALSE);
	mBtnCreateProposal->setVisible(FALSE);

	mBtnViewProposalItem->setEnabled(FALSE);
	mBtnViewProposalItem->setVisible(FALSE);

	mBtnViewProposalList->setEnabled(FALSE);
	mBtnViewProposalList->setVisible(FALSE);

	mQuorumLbl->setVisible(TRUE);
	mQuorum->setEnabled(TRUE);
	mQuorum->setVisible(TRUE);
	mPanel.childSetVisible("quorum_text", TRUE);

	mDurationLbl->setVisible(TRUE);
	mDuration->setEnabled(TRUE);
	mDuration->setVisible(TRUE);
	mDurationText->setVisible(TRUE);

	mMajorityLbl->setVisible(TRUE);
	mMajority->setEnabled(TRUE);
	mMajority->setVisible(TRUE);
	mStartLbl->setVisible(FALSE);
	mStartDate->setVisible(FALSE);
	mEndLbl->setVisible(FALSE);
	mEndDate->setVisible(FALSE);

	if (mQuorum->get() == 0)
	{
		mQuorum->set((F32)mNumGroupMembers);
	}
	mQuorum->setMaxValue((F32)mNumGroupMembers);
}

void LLPanelGroupVoting::impl::setEnableHistoryList()
{
	//we make the assumption here that anyone who has the start
	//or vote powers should also have the view powers

	/* This power was removed to make group roles simpler
	if (!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_VIEW) &&
		!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_VOTE) &&
		!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_START) )
		return;
	*/
	if (!gAgent.isInGroup(mGroupID))
		return;

	mVotesHistoryLbl->setVisible(TRUE);
	mVotesHistory->setVisible(TRUE);
	mVotesHistory->setEnabled(TRUE);

	mVoteHistoryTextLbl->setVisible(FALSE);
	mVoteHistoryText->setVisible(FALSE);	
	mBtnViewHistoryList->setVisible(FALSE);

	mBtnViewHistoryItem->setVisible(TRUE);
	mBtnViewHistoryItem->setEnabled(TRUE);
}

void LLPanelGroupVoting::impl::setEnableHistoryItem()
{
	//we make the assumption here that anyone who has the start
	//or vote powers should also have the view powers
	
	/* This power was removed to make group roles simpler
	if (!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_VIEW) &&
		!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_VOTE) &&
		!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_START) )
		return;
	*/
	
	if (!gAgent.isInGroup(mGroupID))
		return;

	LLScrollListItem *item = mVotesHistory->getFirstSelected();
	// Get full text, not stripped version.
	const LLScrollListCell *cell = item->getColumn(5);
	if (cell)
	{
		mVoteHistoryText->setText(cell->getValue().asString());
	}
	else
	{	// Something's wrong...
		mVoteHistoryText->setText(LLStringUtil::null);
	}
	mVotesHistoryLbl->setVisible(FALSE);
	mVotesHistory->setVisible(FALSE);

	mVoteHistoryTextLbl->setVisible(TRUE);
	mVoteHistoryText->setVisible(TRUE);	
	mVoteHistoryText->setEnabled(FALSE);

	mBtnViewHistoryList->setVisible(TRUE);
	mBtnViewHistoryList->setEnabled(TRUE);
	mBtnViewHistoryItem->setVisible(FALSE);
}

void LLPanelGroupVoting::impl::sendGroupProposalsRequest(const LLUUID& group_id)
{
	//we make the assumption here that anyone who has the start
	//or vote powers should also have the view powers
	
	/* This power was removed to make group roles simpler
	if (!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_VIEW) &&
		!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_VOTE) &&
		!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_START) )
		return;
	*/
	if ( !gAgent.isInGroup(mGroupID) )
		return;

	mProposalTransID.generate();
	mProposals->deleteAllItems(); //this should delete all the objects
	mActiveReceived.clear();
	mOtherGroupHistoryItems = 0;

	//fill in some text so the user will at least know that
	//we're pining the server in high latency situations
	addPendingActiveScrollListItem(0, 0, ADD_BOTTOM);
	mProposals->setCanSelect(FALSE);
	mBtnViewProposalItem->setEnabled(FALSE);

	LLMessageSystem *msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_GroupActiveProposalsRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID() );
	msg->nextBlockFast(_PREHASH_GroupData);
	msg->addUUIDFast(_PREHASH_GroupID, group_id );
	msg->nextBlockFast(_PREHASH_TransactionData);
	msg->addUUIDFast(_PREHASH_TransactionID, mProposalTransID);
	gAgent.sendReliableMessage();
}

void LLPanelGroupVoting::handleResponse(
	const LLUUID& group_id,
	ResponseType response,
	bool success)
{
	impl* self = NULL;

	//see if the voting ballot for the group is still even open
	std::map<LLUUID, LLPanelGroupVoting::impl*>::const_iterator self_iter =
		LLPanelGroupVoting::impl::sGroupIDs.find(group_id);

	if ( LLPanelGroupVoting::impl::sGroupIDs.end() != self_iter )
	{
		//cool, we found the panel's implementation
		//(the panel is still open)
		//then we want to do some other stuff :)
		self = self_iter->second;
	}

	if ( self )
	{
		//refresh the proposals now that we've hit no
		self->sendGroupProposalsRequest(self->mGroupID);
		
		if (response == BALLOT)
		{
			LLSD args;
	
			if (success)
			{
				args["MESSAGE"] = self->mPanel.getString("vote_recorded");
			}
			else
			{
				args["MESSAGE"] = self->mPanel.getString("vote_previously_recorded");
			}

			LLNotificationsUtil::add("SystemMessage", args);

			self->sendGroupVoteHistoryRequest(self->mGroupID);
		}
		self->setEnableListProposals();
	}
}

void LLPanelGroupVoting::handleFailure(
	const LLUUID& group_id)
{
	impl* self = NULL;

	//see if the voting ballot for the group is still even open
	std::map<LLUUID, LLPanelGroupVoting::impl*>::const_iterator self_iter =
		LLPanelGroupVoting::impl::sGroupIDs.find(group_id);

	if ( LLPanelGroupVoting::impl::sGroupIDs.end() != self_iter )
	{
		//cool, we found the panel's implementation
		//(the panel is still open)
		//then we want to do some other stuff :)
		self = self_iter->second;
	}

	if ( self )
	{
		self->setEnableListProposals();		
	}
}

class LLStartGroupVoteResponder : public LLHTTPClient::Responder
{
public:
	LLStartGroupVoteResponder(const LLUUID& group_id)
	{
		mGroupID = group_id;
	}

	//If we get back a normal response, handle it here
	virtual void result(const LLSD& content)
	{
		//Ack'd the proposal initialization, now let's finish up.
		LLPanelGroupVoting::handleResponse(
			mGroupID,
			LLPanelGroupVoting::START_VOTE);
	}

	//If we get back an error (not found, etc...), handle it here
	virtual void error(U32 status, const std::string& reason)
	{
		llinfos << "LLPanelGroupVotingResponder::error "
			<< status << ": " << reason << llendl;

		LLPanelGroupVoting::handleFailure(mGroupID);
	}
private:
	LLUUID mGroupID;
};

class LLGroupProposalBallotResponder : public LLHTTPClient::Responder
{
public:
	LLGroupProposalBallotResponder(const LLUUID& group_id)
	{
		mGroupID = group_id;
	}

	//If we get back a normal response, handle it here
	virtual void result(const LLSD& content)
	{
		//Ack'd the proposal initialization, now let's finish up.
		LLPanelGroupVoting::handleResponse(
			mGroupID,
			LLPanelGroupVoting::BALLOT,
			content["voted"].asBoolean());
	}

	//If we get back an error (not found, etc...), handle it here
	virtual void error(U32 status, const std::string& reason)
	{
		llinfos << "LLPanelGroupVotingResponder::error "
			<< status << ": " << reason << llendl;

		LLPanelGroupVoting::handleFailure(mGroupID);
	}
private:
	LLUUID mGroupID;
};

void LLPanelGroupVoting::impl::sendStartGroupProposal()
{
	if ( !gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_START) )
		return;

	F32 majority = 0.f;
	S32 duration_seconds = (S32)mDuration->get()*24*60*60; // duration days * 24hours/day * 60min/hr * 60sec/min

	switch (mMajority->getSelectedIndex())
	{
	case 0: // Simple Majority
		majority = 0.f;
		break;
	case 1: // 2/3 Majority
		majority = 2.f/3.f;
		break;
	case 2: // Unanimous
		majority = 1.0f;
		break;
	}

	S32 quorum = llfloor(mQuorum->get());

	//*************************************Conversion to capability
	LLSD body;

	std::string url = gAgent.getRegion()->getCapability("StartGroupProposal");

	if (!url.empty())
	{
		body["agent-id"]		= gAgent.getID();
		body["session-id"]		= gAgent.getSessionID();

		body["group-id"]		= mGroupID;
		body["majority"]		= majority;
		body["quorum"]			= quorum;
		body["duration"]		= duration_seconds;
		body["proposal-text"]	= mProposalText->getText();

		LLHTTPClient::post(
			url,
			body,
			new LLStartGroupVoteResponder(mGroupID),
			300);
	}
	else
	{	//DEPRECATED!!!!!!!  This is a fallback just in case our backend cap is not there.  Delete this block ASAP!
		LLMessageSystem *msg = gMessageSystem;

		msg->newMessageFast(_PREHASH_StartGroupProposal);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID() );

		msg->nextBlockFast(_PREHASH_ProposalData);
		msg->addUUIDFast(_PREHASH_GroupID, mGroupID);
		msg->addF32Fast(_PREHASH_Majority, majority );
		msg->addS32Fast(_PREHASH_Quorum, quorum );
		msg->addS32Fast(_PREHASH_Duration, duration_seconds );
		msg->addStringFast(_PREHASH_ProposalText, mProposalText->getText());

		gAgent.sendReliableMessage();

		//This code was moved from the callers to here as part of deprecation.
		sendGroupProposalsRequest(mGroupID);
		setEnableListProposals();
	}
}

void LLPanelGroupVoting::impl::sendGroupProposalBallot(const std::string& vote)
{
	if ( !gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_VOTE) )
		return;
	
	LLSD body;

	std::string url = gAgent.getRegion()->getCapability("GroupProposalBallot");

	if (!url.empty())
	{
		body["agent-id"]		= gAgent.getID();
		body["session-id"]		= gAgent.getSessionID();
		body["proposal-id"]		= mProposalID;
		body["group-id"]		= mGroupID;
		body["vote"]	= vote;

		LLHTTPClient::post(
			url,
			body,
			new LLGroupProposalBallotResponder(mGroupID),
			300);
	}
	else
	{	//DEPRECATED!!!!!!!  This is a fallback just in case our backend cap is not there.  Delete this block ASAP!
		LLMessageSystem *msg = gMessageSystem;

		msg->newMessageFast(_PREHASH_GroupProposalBallot);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID() );

		msg->nextBlockFast(_PREHASH_ProposalData);
		msg->addUUIDFast(_PREHASH_ProposalID, mProposalID);
		msg->addUUIDFast(_PREHASH_GroupID, mGroupID);
		msg->addStringFast(_PREHASH_VoteCast, vote);

		gAgent.sendReliableMessage();

		//This code was moved from the callers to here as part of deprecation.
		sendGroupProposalsRequest(mGroupID);
		sendGroupVoteHistoryRequest(mGroupID);
		setEnableListProposals();
	}
}

void LLPanelGroupVoting::impl::sendGroupVoteHistoryRequest(const LLUUID& group_id)
{
	//we make the assumption here that anyone who has the start
	//or vote powers should also have the view powers
	
	/* This power was removed to make group roles simpler
	if (mAwaitingFirstHistoryItem ||
		(!gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_VIEW) &&
		 !gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_VOTE) &&
		 !gAgent.hasPowerInGroup(mGroupID, GP_PROPOSAL_START)) )
		return;
	*/

	if (!gAgent.isInGroup(mGroupID))
		return;

	mVotesHistory->deleteAllItems();
	mHistoryTransID.generate();
	mHistoryReceived.clear();

	//add some text so the user knows we're doing something
	addPendingHistoryScrollListItem(0, 0, ADD_BOTTOM);
	mVotesHistory->setCanSelect(FALSE);
	mBtnViewHistoryItem->setEnabled(FALSE);

	LLMessageSystem *msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_GroupVoteHistoryRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID() );
	msg->nextBlockFast(_PREHASH_GroupData);
	msg->addUUIDFast(_PREHASH_GroupID, group_id );
	msg->nextBlockFast(_PREHASH_TransactionData);
	msg->addUUIDFast(_PREHASH_TransactionID, mHistoryTransID);
	gAgent.sendReliableMessage();
}

void LLPanelGroupVoting::impl::updateQuorumText()
{
	mPanel.childSetTextArg("quorum_text", "[MEMBERS]", llformat("%d", mNumGroupMembers));
}

void LLPanelGroupVoting::impl::addPendingActiveScrollListItem(unsigned int current,
															  unsigned int expected,
															  EAddPosition pos)
{
	std::stringstream pending;
	//*TODO: translate
	pending << "Retrieving active proposals ("
			<< current
			<< "\\" << expected  << ")";

	mProposals->addCommentText(pending.str());
}

void LLPanelGroupVoting::impl::addNoActiveScrollListItem(EAddPosition pos)
{
	//*TODO: translate
	mProposals->addCommentText(std::string("There are currently no active proposals"), pos);
}

void LLPanelGroupVoting::impl::addNoHistoryScrollListItem(EAddPosition pos)
{
	//*TODO: translate
	mVotesHistory->addCommentText(std::string("There are currently no archived proposals"), pos);
}

void LLPanelGroupVoting::impl::addPendingHistoryScrollListItem(unsigned int current,
															  unsigned int expected,
															  EAddPosition pos)
{
	//*TODO: translate
	std::stringstream pending;
	pending << "Retrieving archived proposals ("
			<< current
			<< "\\" << expected  << ")";

	mVotesHistory->addCommentText(pending.str());
}
																		

//static
std::map<LLUUID, LLPanelGroupVoting::impl*> LLPanelGroupVoting::impl::sGroupIDs;

static std::string format_date(std::string date)
{
	tm t;
	if (sscanf(date.c_str(), "%u/%u/%u %u:%u",
		&t.tm_mon, &t.tm_mday, &t.tm_year, &t.tm_hour, &t.tm_min) == 5
		&& t.tm_year > 1900)
	{
		t.tm_year -= 1900;
		t.tm_mon--;
		t.tm_sec = 0;
		std::string format = gSavedSettings.getString("ShortDateFormat");
		format += " " + gSavedSettings.getString("ShortTimeFormat");
		timeStructToFormattedString(&t, format, date);
	}
	return date;
}

//static
void LLPanelGroupVoting::impl::processGroupActiveProposalItemReply(LLMessageSystem *msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );
	if (gAgent.getID() != agent_id)
	{
		llwarns << "Got active group proposals reply for another agent!" << llendl;
		return;
	}

	LLUUID group_id;
	LLUUID trans_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_GroupID, group_id );
	impl* self = get_if_there(sGroupIDs, group_id, (impl*) NULL);
	if (!self) 
	{
		return;
	}

	msg->getUUIDFast(_PREHASH_TransactionData,
					 _PREHASH_TransactionID,
					 trans_id);

	if ( self->mProposalTransID != trans_id ) return;


	U32 num_expected;
	std::string item_num_string;
	std::string proposal_text;
	std::string vote_cast;
	std::string start_datetime;
	std::string end_datetime;
	std::string vote_type;
	std::string majority_text;
	std::string quorum_text;
	std::string vote_initiator_string;

	LLUUID vote_id;
	LLUUID vote_initiator;

	F32  majority;
	S32  quorum;
	BOOL already_voted;
	S32 num_proposals = msg->getNumberOfBlocksFast(_PREHASH_ProposalData);
	
	msg->getU32Fast(_PREHASH_TransactionData,
					_PREHASH_TotalNumItems,
					num_expected);

	//the first item should be indicative that we
	//are currently receiving information or have it all
	self->mProposals->deleteAllItems();
	// make sure column indices are correct when populating with real data
	self->mProposals->clearColumns();

	for (int i = 0; i < num_proposals; i++)
	{
		msg->getStringFast(_PREHASH_ProposalData, _PREHASH_TerseDateID, item_num_string, i );
		msg->getUUIDFast(_PREHASH_ProposalData, _PREHASH_VoteID,	vote_id, i );
		msg->getStringFast(_PREHASH_ProposalData, _PREHASH_StartDateTime, start_datetime, i );
		msg->getStringFast(_PREHASH_ProposalData, _PREHASH_EndDateTime, end_datetime, i );
		msg->getUUIDFast(_PREHASH_ProposalData, _PREHASH_VoteInitiator, vote_initiator, i );
		msg->getBOOLFast(_PREHASH_ProposalData,_PREHASH_AlreadyVoted, already_voted, i );
		msg->getStringFast(_PREHASH_ProposalData,_PREHASH_VoteCast, vote_cast, i );
		msg->getStringFast(_PREHASH_ProposalData, _PREHASH_ProposalText, proposal_text, i );
		msg->getF32Fast(_PREHASH_ProposalData, _PREHASH_Majority, majority, i );
		msg->getS32Fast(_PREHASH_ProposalData, _PREHASH_Quorum, quorum, i );

		vote_initiator.toString(vote_initiator_string);
		majority_text = llformat("%f", majority);
		quorum_text = llformat("%i", quorum);

		LLSD row;
		S32 index = 0;
		row["id"] = vote_id;
		row["columns"][0]["column"] = "item_num";
		row["columns"][0]["value"] = item_num_string;
		row["columns"][0]["font"] = "SANSSERIF_SMALL";
		row["columns"][0]["width"] = self->mProposalColumnWidths[index++];

		row["columns"][1]["column"] = "proposal_text";
		row["columns"][1]["value"] = proposal_text;
		row["columns"][1]["font"] = "SANSSERIF_SMALL";
		row["columns"][1]["width"] = self->mProposalColumnWidths[index++];

		row["columns"][2]["column"] = "end_datetime";
		row["columns"][2]["value"] = format_date(end_datetime);
		row["columns"][2]["font"] = "SANSSERIF_SMALL";
		row["columns"][2]["width"] = self->mProposalColumnWidths[index++];

		row["columns"][3]["column"] = "vote_type";
		row["columns"][3]["value"] = vote_type;
		row["columns"][3]["font"] = "SANSSERIF_SMALL";
		row["columns"][3]["width"] = self->mProposalColumnWidths[index++];

		row["columns"][4]["column"] = "already_voted";
		row["columns"][4]["value"] = already_voted ? "Yes" : "No";
		row["columns"][4]["font"] = "SANSSERIF_SMALL";
		row["columns"][4]["width"] = self->mProposalColumnWidths[index++];

		row["columns"][5]["column"] = "start_datetime";
		row["columns"][5]["value"] = format_date(start_datetime);
		row["columns"][5]["font"] = "SANSSERIF_SMALL";
		row["columns"][5]["width"] = self->mProposalColumnWidths[index++];

		row["columns"][6]["column"] = "vote_cast";
		row["columns"][6]["value"] = vote_cast;
		row["columns"][6]["font"] = "SANSSERIF_SMALL";
		row["columns"][6]["width"] = self->mProposalColumnWidths[index++];

		row["columns"][7]["column"] = "vote_initator_string";
		row["columns"][7]["value"] = vote_initiator_string;
		row["columns"][7]["font"] = "SANSSERIF_SMALL";
		row["columns"][7]["width"] = self->mProposalColumnWidths[index++];

		row["columns"][8]["column"] = "quorum_text";
		row["columns"][8]["value"] = quorum_text;
		row["columns"][8]["font"] = "SANSSERIF_SMALL";
		row["columns"][8]["width"] = self->mProposalColumnWidths[index++];

		row["columns"][9]["column"] = "majority_text";
		row["columns"][9]["value"] = majority_text;
		row["columns"][9]["font"] = "SANSSERIF_SMALL";
		row["columns"][9]["width"] = self->mProposalColumnWidths[index++];
		
		self->mActiveReceived.push_back(row);
	}

	int received = self->mActiveReceived.size();
	if ( num_expected == 0 )
	{
		//we are expecting 0 items, put up a message indicating tehre are
		//no active proposals and make the scroll list unselectable
		self->addNoActiveScrollListItem(ADD_BOTTOM);
		self->mProposals->setCanSelect(FALSE);
		self->mBtnViewProposalItem->setEnabled(FALSE);
	}
	else if ( (U32)received != num_expected )
	{
		self->addPendingActiveScrollListItem(received,
											 num_expected,
											 ADD_BOTTOM);
		self->mProposals->setCanSelect(FALSE);
		self->mBtnViewProposalItem->setEnabled(FALSE);
	}
	else
	{
		//all done display all of the items
		std::vector<LLSD>::iterator it = self->mActiveReceived.begin();
		std::vector<LLSD>::iterator end = self->mActiveReceived.end();

		for (; it != end; it++)
		{
			self->mProposals->addElement((*it), ADD_SORTED);
		}

		self->mProposals->setCanSelect(TRUE);
		self->mBtnViewProposalItem->setEnabled(TRUE);
	}
}

//static
void LLPanelGroupVoting::impl::processGroupVoteHistoryItemReply(LLMessageSystem *msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );
	if (gAgent.getID() != agent_id)
	{
		llwarns << "Got group voting history reply for another agent!" << llendl;
		return;
	}

	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_GroupID, group_id );
	impl* self = sGroupIDs[group_id];
	if (!self) 
	{
		return;
	}

	LLUUID trans_id;
	U32	num_expected;
	
	msg->getU32Fast(_PREHASH_TransactionData,
					_PREHASH_TotalNumItems,
					num_expected);
	msg->getUUIDFast(_PREHASH_TransactionData,
					 _PREHASH_TransactionID,
					 trans_id);

	if ( self->mHistoryTransID != trans_id ) return;

	//the first item should be some text
	//explaining that we are attempting to receive information
	//or are currently receiving information, so we should clear out
	//the first line
	self->mVotesHistory->deleteAllItems();
	// make sure column indices are correct when populating with real data
	self->mVotesHistory->clearColumns();

	if ( num_expected == 0 )
	{
		//we are expecting 0 items, put up a message indicating tehre are
		//no active proposals and make the scroll list unselectable
		self->addNoHistoryScrollListItem(ADD_BOTTOM);
		self->mVotesHistory->setCanSelect(FALSE);
		self->mBtnViewHistoryItem->setEnabled(FALSE);
		return;
	}

	std::string item_num_string;
	std::string proposal_text;
	std::string vote_result;
	std::string start_datetime;
	std::string end_datetime;
	std::string vote_type;

	std::string vote_text;
	LLUUID vote_id;
	LLUUID vote_initiator;
	LLUUID winner_id;

	F32	majority;
	S32	quorum;

	msg->getStringFast(_PREHASH_HistoryItemData, _PREHASH_TerseDateID, item_num_string );
	msg->getUUIDFast(_PREHASH_HistoryItemData, _PREHASH_VoteID,	vote_id );
	msg->getStringFast(_PREHASH_HistoryItemData, _PREHASH_StartDateTime, start_datetime );
	msg->getStringFast(_PREHASH_HistoryItemData, _PREHASH_EndDateTime, end_datetime );
	msg->getUUIDFast(_PREHASH_HistoryItemData, _PREHASH_VoteInitiator, vote_initiator);
	msg->getStringFast(_PREHASH_HistoryItemData, _PREHASH_VoteType, vote_type);
	msg->getStringFast(_PREHASH_HistoryItemData,_PREHASH_VoteResult, vote_result);
	msg->getStringFast(_PREHASH_HistoryItemData, _PREHASH_ProposalText, proposal_text );
	msg->getF32Fast(_PREHASH_HistoryItemData, _PREHASH_Majority, majority);
	msg->getS32Fast(_PREHASH_HistoryItemData, _PREHASH_Quorum, quorum);

	S32 vote_items = msg->getNumberOfBlocksFast(_PREHASH_VoteItem);

	if (vote_items > 0)
	{
		if (vote_type == "Proposal")
		{
			LLSD row;
			row["id"] = vote_id;

			S32 index = 0;
			row["columns"][0]["column"] = "item_num";
			row["columns"][0]["value"] = item_num_string;
			row["columns"][0]["font"] = "SANSSERIF_SMALL";
			row["columns"][0]["width"] = self->mHistoryColumnWidths[index++];
			
			vote_text.assign(proposal_text);
			vote_text.append("\n\n--\n");
			if (vote_result == "Success")
			{
				vote_text.append("Proposal PASSED.");
			}
			else
			{
				vote_text.append("Proposal FAILED.");
			}
			vote_text.append("  Votes->\n");

			std::string vote_result;
			S32 num_votes;

			for (S32 i = 0; i < vote_items; ++i)
			{
				msg->getStringFast(_PREHASH_VoteItem, _PREHASH_VoteCast, vote_result, i);
				msg->getS32Fast(_PREHASH_VoteItem, _PREHASH_NumVotes, num_votes, i);
				vote_text.append(llformat("    %s: %d\n", vote_result.c_str(), num_votes));
			}

			vote_text.append("\n");

			std::string vote_text_stripped = vote_text;
			LLStringUtil::stripNonprintable(vote_text_stripped);

			row["columns"][1]["column"] = "vote_text_stripped";
			row["columns"][1]["value"] = vote_text_stripped;
			row["columns"][1]["font"] = "SANSSERIF_SMALL";
			row["columns"][1]["width"] = self->mHistoryColumnWidths[index++];

			row["columns"][2]["column"] = "end_datetime";
			row["columns"][2]["value"] = format_date(end_datetime);
			row["columns"][2]["font"] = "SANSSERIF_SMALL";
			row["columns"][2]["width"] = self->mHistoryColumnWidths[index++];

			row["columns"][3]["column"] = "vote_type";
			row["columns"][3]["value"] = vote_type;
			row["columns"][3]["font"] = "SANSSERIF_SMALL";
			row["columns"][3]["width"] = self->mHistoryColumnWidths[index++];

			row["columns"][4]["column"] = "vote_result";
			row["columns"][4]["value"] = vote_result;
			row["columns"][4]["font"] = "SANSSERIF_SMALL";
			row["columns"][4]["width"] = self->mHistoryColumnWidths[index++];

			row["columns"][5]["column"] = "vote_text";
			row["columns"][5]["value"] = vote_text;
			row["columns"][5]["font"] = "SANSSERIF_SMALL";
			row["columns"][5]["width"] = self->mHistoryColumnWidths[index++];

			//row->addColumn(vote_text_stripped, font, self->mHistoryColumnWidths[1]);
			//row->addColumn(end_datetime, font, self->mHistoryColumnWidths[2]);
			//row->addColumn(vote_type, font, self->mHistoryColumnWidths[3]);
			//row->addColumn(vote_result, font, self->mHistoryColumnWidths[4]);
			//row->addColumn(vote_text, font, self->mHistoryColumnWidths[5]);

			self->mHistoryReceived.push_back(row);
		} //end if proposal
		else
		{
			llinfos << "Vote is not a proposal, but a " << vote_type << llendl;
			self->mOtherGroupHistoryItems++;
		}
	} //end if vote_items > 0

	int received = self->mHistoryReceived.size();
	if ( (U32)received + self->mOtherGroupHistoryItems != num_expected )
	{
		self->addPendingHistoryScrollListItem(received,
											  num_expected,
											  ADD_BOTTOM);
		self->mVotesHistory->setCanSelect(FALSE);
		self->mBtnViewHistoryItem->setEnabled(FALSE);
	}
	else
	{
		//all done display all of the items
		std::vector<LLSD>::iterator it = self->mHistoryReceived.begin();
		std::vector<LLSD>::iterator end = self->mHistoryReceived.end();

		for (; it != end; it++)
		{
			self->mVotesHistory->addElement((*it), ADD_SORTED);
		}

		self->mVotesHistory->setCanSelect(TRUE);
		self->mBtnViewHistoryItem->setEnabled(TRUE);
	}
}

//static
void LLPanelGroupVoting::impl::onClickCreateProposal(void* userdata)
{
	impl* self = (impl*)userdata;
	if ( self ) self->setEnableCreateProposal();
}

//static
void LLPanelGroupVoting::impl::onClickYes(void *userdata)
{
	LLPanelGroupVoting::impl* self = (LLPanelGroupVoting::impl*)userdata;

	if ( self )
	{
		self->mPanel.childSetText("proposal_instructions", self->mPanel.getString("proposals_submit_yes_txt"));
		self->sendGroupProposalBallot("Yes");
	}
}

//static
void LLPanelGroupVoting::impl::onClickNo(void *userdata)
{
	LLPanelGroupVoting::impl* self = (LLPanelGroupVoting::impl*)userdata;

	if ( self )
	{
		self->mPanel.childSetText("proposal_instructions", self->mPanel.getString("proposals_submit_no_txt"));
		self->sendGroupProposalBallot("No");
	}
}

//static
void LLPanelGroupVoting::impl::onClickAbstain(void *userdata)
{
	impl* self = (LLPanelGroupVoting::impl*) userdata;

	if ( self )
	{
		self->mPanel.childSetText("proposal_instructions", self->mPanel.getString("proposals_submit_abstain_txt"));
		self->sendGroupProposalBallot("Abstain");
	}
}


//static
void LLPanelGroupVoting::impl::onClickSubmitProposal(void *userdata)
{
	gFocusMgr.setKeyboardFocus(NULL); 
	impl* self = (impl*)userdata;

	if ( self && self->mProposalText )
	{
		//check to see if the proposal has any text for some form of
		//"validation"
		if ( self->mProposalText->getText().empty() )
		{
			//throw up an error dialog
			LLSD args;
			args["MESSAGE"] = self->mPanel.getString("empty_proposal_txt");
			LLNotificationsUtil::add("GenericAlert", args);
			return;
		}

		self->mPanel.childSetText("proposal_instructions", self->mPanel.getString("proposals_submit_new_txt"));
		self->sendStartGroupProposal();
	}
}

//static
void LLPanelGroupVoting::impl::onClickCancelProposal(void* userdata)
{
	impl* self = (impl*)userdata;

	if ( self )
	{
		self->setEnableListProposals();
	}
}

//static
void LLPanelGroupVoting::impl::onClickViewProposalList(void *userdata)
{
	impl* self = (LLPanelGroupVoting::impl*)userdata;

	if ( self ) self->setEnableListProposals();
}

//static
void LLPanelGroupVoting::impl::onClickViewProposalItem(void *userdata)
{
	impl* self = (LLPanelGroupVoting::impl*)userdata;

	if ( self && self->mProposals )
	{
		LLScrollListItem * proposal = self->mProposals->getFirstSelected();
		// Check if it has anything in column 2.  If not, assume it's the "There are currently no active proposals" text
		if (proposal && proposal->getColumn(2))
		{
			self->setEnableVoteProposal();
		}
		else
		{
			LLNotificationsUtil::add("SelectProposalToView");
		}
	}
}

//static
void LLPanelGroupVoting::impl::onDoubleClickProposal(void *userdata)
{
	impl* self = (impl*)userdata;
	if ( self && self->mProposals->getFirstSelected() != NULL )
	{
		self->setEnableVoteProposal();
	}
}

//static
void LLPanelGroupVoting::impl::onDoubleClickHistoryItem(void *userdata)
{
	impl* self = (impl*)userdata;
	if ( self && self->mVotesHistory->getFirstSelected() != NULL )
	{
		self->setEnableHistoryItem();
	}
}

//static
void LLPanelGroupVoting::impl::onClickViewHistoryItem(void *userdata)
{
	impl* self = (impl*)userdata;

	if ( self && self->mVotesHistory )
	{
		LLScrollListItem * historic = self->mVotesHistory->getFirstSelected();
		// Check if it has anything in column 2.  If not, assume it's the "There are currently no active proposals" text
		if (historic && historic->getColumn(2))
		{
			self->setEnableHistoryItem();
		}
		else
		{
			LLNotificationsUtil::add("SelectHistoryItemToView");
		}
	}
}

//static
void LLPanelGroupVoting::impl::onClickViewHistoryList(void *userdata)
{
	impl* self = (impl*)userdata;

	if ( self) self->setEnableHistoryList();
}

//**********************************
//** LLPanelGroupVoting Functions **
//**********************************

// static
void* LLPanelGroupVoting::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupVoting("panel group voting", *group_id);
}

LLPanelGroupVoting::LLPanelGroupVoting(const std::string& name, 
								  const LLUUID& group_id)
	: LLPanelGroupTab(name, group_id)
{
	mImpl = new impl(*this, group_id);
}

LLPanelGroupVoting::~LLPanelGroupVoting()
{
	delete mImpl;
}

BOOL LLPanelGroupVoting::isVisibleByAgent(LLAgent* agentp)
{
	 //if they are in the group, the panel is viewable
	return mAllowEdit && agentp->isInGroup(mGroupID);
}

BOOL LLPanelGroupVoting::postBuild()
{
	bool recurse = true;

	mImpl->mDurationText = getChild<LLTextBox>("duration_text",
													   recurse);
	mImpl->mQuorum       = getChild<LLSpinCtrl>("quorum", recurse);
	mImpl->mQuorumLbl    = getChild<LLTextBox>("quorum_lbl", recurse);
	mImpl->mDuration     = getChild<LLSpinCtrl>("duration", recurse);
	mImpl->mDurationLbl  = getChild<LLTextBox>("duration_lbl", recurse);
	mImpl->mMajority     = getChild<LLRadioGroup>("majority", recurse);
	mImpl->mMajorityLbl  = getChild<LLTextBox>("majority_lbl", recurse);

	mImpl->mStartLbl     = getChild<LLTextBox>("start_lbl", recurse);
	mImpl->mEndLbl       = getChild<LLTextBox>("end_lbl", recurse);
	mImpl->mStartDate    = getChild<LLTextBox>("start_date", recurse);
	mImpl->mEndDate      = getChild<LLTextBox>("end_date", recurse);

	mImpl->mBtnYes     = getChild<LLButton>("btn_yes", recurse);
	mImpl->mBtnNo      = getChild<LLButton>("btn_no", recurse);
	mImpl->mBtnAbstain = getChild<LLButton>("btn_abstain", recurse);

	mImpl->mProposals =
		getChild<LLScrollListCtrl>("proposals", recurse);
	mImpl->mProposalText =
		getChild<LLTextEditor>("proposal_text", recurse);

	mImpl->mBtnCreateProposal = getChild<LLButton>("btn_proposal",
														   recurse);
	mImpl->mBtnSubmitProposal = getChild<LLButton>("btn_submit",
														   recurse);
	mImpl->mBtnCancelProposal = getChild<LLButton>("btn_cancel",
														   recurse);

	mImpl->mBtnViewProposalList =
		getChild<LLButton>("btn_view_proposal_list", recurse);
	mImpl->mBtnViewProposalItem =
		getChild<LLButton>("btn_view_proposal_item", recurse);

	mImpl->mVotesHistory       = getChild<LLScrollListCtrl>("history_list",
																   recurse);
	mImpl->mVotesHistoryLbl    = getChild<LLTextBox>("history_list_lbl",
															 recurse);
	mImpl->mVoteHistoryText    = getChild<LLTextEditor>("vote_text",
																recurse);
	mImpl->mVoteHistoryTextLbl = getChild<LLTextBox>("vote_text_lbl",
															 recurse);

	mImpl->mBtnViewHistoryList =
		getChild<LLButton>("btn_view_history_list", recurse);
	mImpl->mBtnViewHistoryItem =
		getChild<LLButton>("btn_view_history_item", recurse);

	mImpl->updateQuorumText();

	bool success = (mImpl->mDurationText &&
					mImpl->mQuorum &&
					mImpl->mQuorumLbl &&
					mImpl->mDuration &&
					mImpl->mDurationLbl &&
					mImpl->mMajority &&
					mImpl->mMajorityLbl &&
					mImpl->mStartLbl &&
					mImpl->mEndLbl &&
					mImpl->mStartDate &&
					mImpl->mEndDate &&
					mImpl->mBtnYes &&
					mImpl->mBtnNo &&
					mImpl->mBtnAbstain &&
					mImpl->mProposals &&
					mImpl->mProposalText &&
					mImpl->mBtnCreateProposal &&
					mImpl->mBtnSubmitProposal &&
					mImpl->mBtnCancelProposal &&
					mImpl->mBtnViewProposalList &&
					mImpl->mBtnViewProposalItem &&
					mImpl->mVotesHistory &&
					mImpl->mVotesHistoryLbl &&
					mImpl->mVoteHistoryText &&
					mImpl->mVoteHistoryTextLbl &&
					mImpl->mBtnViewHistoryList &&
					mImpl->mBtnViewHistoryItem );

	//associate callbacks
	if ( success )
	{
		mImpl->mProposals->setDoubleClickCallback(impl::onDoubleClickProposal);
		mImpl->mProposals->setCallbackUserData(mImpl);

		mImpl->mVotesHistory->setDoubleClickCallback(impl::onDoubleClickHistoryItem);
		mImpl->mVotesHistory->setCallbackUserData(mImpl);

		mImpl->mBtnAbstain->setClickedCallback(impl::onClickAbstain);
		mImpl->mBtnAbstain->setCallbackUserData(mImpl);

		mImpl->mBtnNo->setClickedCallback(impl::onClickNo);
		mImpl->mBtnNo->setCallbackUserData(mImpl);

		mImpl->mBtnYes->setClickedCallback(impl::onClickYes);
		mImpl->mBtnYes->setCallbackUserData(mImpl);

		mImpl->mBtnCreateProposal->setClickedCallback(impl::onClickCreateProposal);
		mImpl->mBtnCreateProposal->setCallbackUserData(mImpl);

		mImpl->mBtnSubmitProposal->setClickedCallback(impl::onClickSubmitProposal);
		mImpl->mBtnSubmitProposal->setCallbackUserData(mImpl);

		mImpl->mBtnCancelProposal->setClickedCallback(impl::onClickCancelProposal);
		mImpl->mBtnCancelProposal->setCallbackUserData(mImpl);

		mImpl->mBtnViewProposalList->setClickedCallback(impl::onClickViewProposalList);
		mImpl->mBtnViewProposalList->setCallbackUserData(mImpl);

		mImpl->mBtnViewProposalItem->setClickedCallback(impl::onClickViewProposalItem);
		mImpl->mBtnViewProposalItem->setCallbackUserData(mImpl);

		mImpl->mBtnViewHistoryList->setClickedCallback(impl::onClickViewHistoryList);
		mImpl->mBtnViewHistoryList->setCallbackUserData(mImpl);

		mImpl->mBtnViewHistoryItem->setClickedCallback(impl::onClickViewHistoryItem);
		mImpl->mBtnViewHistoryItem->setCallbackUserData(mImpl);

		gMessageSystem->setHandlerFuncFast(_PREHASH_GroupActiveProposalItemReply, 
										   impl::processGroupActiveProposalItemReply);
		gMessageSystem->setHandlerFuncFast(_PREHASH_GroupVoteHistoryItemReply, 
										   impl::processGroupVoteHistoryItemReply);
	}

	return success && LLPanelGroupTab::postBuild();
}


void LLPanelGroupVoting::activate()
{
	LLGroupMgr::getInstance()->sendGroupPropertiesRequest(mGroupID);
	update(GC_ALL);

	mImpl->setEnableListProposals();
	mImpl->setEnableHistoryList();
}

bool LLPanelGroupVoting::needsApply(std::string& mesg)
{
	return false;
}

bool LLPanelGroupVoting::apply(std::string& mesg)
{
	return true;
}

void LLPanelGroupVoting::update(LLGroupChange gc)
{
	LLGroupMgrGroupData* group_datap = NULL;
	group_datap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if ( group_datap )
	{
		S32 num_group_members = group_datap->mMemberCount;
		mImpl->mNumGroupMembers = num_group_members;

		mImpl->updateQuorumText();
	}

	mImpl->sendGroupProposalsRequest(mGroupID);
	mImpl->sendGroupVoteHistoryRequest(mGroupID);
}


