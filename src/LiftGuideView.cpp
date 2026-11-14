#include "LiftGuideView.h"
LiftGuideView liftGuideView(LiftGuideStage stage, bool applied, bool durable) {
  using S=LiftGuideStage; using A=LiftGuideAction;
  LiftGuideView view;
  auto add=[&](A action,const char *label){view.choices[view.count++]={action,label};};
  bool measuring=false;
  switch(stage) {
    case S::reference:
      view.title="Confirm fully open";view.hint="Watch the shutter. Confirm only at its physical endpoint.";
      add(A::open,"Open shutter");add(A::confirmOpen,"It is fully open");measuring=true;break;
    case S::readyDown:
      view.title="Measure closing";view.hint="Stand where the full shutter is visible.";
      add(A::startDown,"Start closing");break;
    case S::readyUp:
      view.title="Measure opening";view.hint="The closed endpoint is confirmed. Measure the other direction.";
      add(A::startUp,"Start opening");break;
    case S::startingDown:case S::startingUp:
      view.title="Waiting for RF start";view.hint="Timing starts when the radio actually transmits.";measuring=true;break;
    case S::downTakeup:case S::upTakeup:
      view.title=stage==S::downTakeup?"Closing: take-up":"Opening: take-up";
      view.hint="Press as the shutter itself begins to move.";
      add(A::moving,"Visible movement began");measuring=true;break;
    case S::downHalf:case S::upHalf:
      view.title=stage==S::downHalf?"Closing: halfway":"Opening: halfway";
      view.hint="Press when visible travel reaches halfway in this direction.";
      add(A::halfway,"At halfway");measuring=true;break;
    case S::downEnd:case S::upEnd:
      view.title=stage==S::downEnd?"Closing: visible end":"Opening: visible end";
      view.hint="Press when visible travel ends, before final settling.";
      add(A::visibleEnd,"Visible travel ended");measuring=true;break;
    case S::downConfirm:case S::upConfirm:
      view.title=stage==S::downConfirm?"Confirm fully closed":"Confirm fully open";
      view.hint="Wait for all gaps to settle and the motor to stop.";
      add(A::confirmEndpoint,"Endpoint reached; stopped");measuring=true;break;
    case S::review:
      view.title="Review calibration";view.hint="Review both directions. Verify positions after saving.";
      add(A::save,"Save calibration");break;
    case S::saving:view.title="Saving calibration";view.hint="Waiting for the settings file result.";return view;
    case S::complete:
      view.title="Calibration saved";view.hint="Test 25%, 50%, 75% individually, then test the group.";
      add(A::cancel,"Close");return view;
    case S::failed:
      view.title=applied&&!durable?"Applied; save failed":"Calibration interrupted";
      view.hint=applied&&!durable?"Running profile changed. Retry COMMIT in storage recovery.":
        "Draft cannot be saved. A command may still run to its end limit.";
      add(A::cancel,"Close");return view;
    case S::closed:return view;
  }
  if(measuring)add(A::stop,"Stop moving shutter");
  add(A::cancel,"Discard draft");
  return view;
}
