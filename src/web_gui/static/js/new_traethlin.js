// const ROSLIB = require("roslib/src/RosLib");

const joystick = document.getElementById('traethlin-joystick');
const joystick_wrapper = document.getElementById('joystick_wrapper');
const startstopButton = document.getElementById("startstop");
const shutdownButton = document.getElementById("shutdown");
//const rebootButton = document.getElementById("reboot");
var gearCheckbox = document.getElementById('high_low_gear');
var frontDiffCheckbox = document.getElementById('front_diff');
var rearDiffCheckbox = document.getElementById('rear_diff');

var ros = new ROSLIB.Ros();

// Listen to mouse and touch events
joystick.addEventListener('touchstart', mouseDownEvent);
document.addEventListener('touchmove', mouseMoveEvent);
document.addEventListener('touchend', mouseUpEvent);

document.addEventListener('mousemove', mouseMoveEvent);
joystick.addEventListener('mousedown', mouseDownEvent);
document.addEventListener('mouseup', mouseUpEvent);


startstopButton.addEventListener("click", startstop);
shutdownButton.addEventListener("click", shutdown);
//rebootButton.addEventListener("click", reboot);

gearCheckbox.addEventListener('change', function() {
  if (this.checked) {
    shiftGear(1);
  } else {
    shiftGear(0);
  }
});

frontDiffCheckbox.addEventListener('change', function() {
  lockFrontDifferential(this.checked);
});

rearDiffCheckbox.addEventListener('change', function() {
  lockRearDifferential(this.checked);
});



joystickPosition = { x: 0.0, y: 0.0};
joystickPositionOffset = {x: 0.0, y: 0.0};
joystickActive = false
const maxDiff = 360.0;

var pollJoystickIntervalTimer = null;

function mouseDownEvent(event) {
    joystick_wrapper.style.opacity = '100%';

    if (event.changedTouches) {
        joystickPositionOffset = {
            x : event.changedTouches[0].clientX,
            y : event.changedTouches[0].clientY
        };
    } else {
        joystickPositionOffset = {
            x : event.clientX,
            y : event.clientY
        }
    }
    joystickActive = true;
    pollJoystickIntervalTimer = window.setInterval(pollJoystick, 100); // milliseconds
}

function mouseMoveEvent(event) {
    if (!joystickActive) {
        return;
    }

    // Don't use normal mouse event handling
    event.preventDefault();

    if (event.changedTouches) {
        event.clientX = event.changedTouches[0].clientX;
        event.clientY = event.changedTouches[0].clientY;
    }


    const xDiff = (event.clientX - joystickPositionOffset.x);
    const yDiff = (event.clientY - joystickPositionOffset.y);
    const angle = Math.atan2(yDiff, xDiff);
    const distance = Math.min(maxDiff, Math.hypot(xDiff, yDiff));

    joystickPosition = {
        x : distance * Math.cos(angle),
        y : distance * Math.sin(angle)
    };

    joystick.style.transition = '.05s';
    joystick.style.transform = `translate3d(${joystickPosition.x}px, ${joystickPosition.y}px, 0px)`;
}

function mouseUpEvent(event) {
    if (!joystickActive) {
        return;
    }

    joystick_wrapper.style.opacity = '80%';

    // Reset internal position of joystick
    joystickPosition = { x: 0, y: 0 };

    // Reset visual position of
    joystick.style.transition = '.3s';
    joystick.style.transform = `translate3d(0px, 0px, 0px)`;

    joystickActive = false;
    window.clearInterval(pollJoystickIntervalTimer);
}

function pollJoystick() {
    move(
        max_linear * -joystickPosition.y / maxDiff,
        max_angular * -joystickPosition.x / maxDiff
        );
}

var isRosConnected = 0;

function startstop() {
    if (!isRosConnected) {
        ros.connect("ws://" + window.location.hostname + ":9090");
        isRosConnected = 1;
    } else {
        ros.close();
        isRosConnected = 0;
    }
}

function greyOutElements() {
    joystick.style.backgroundColor = "#bbb";
    joystick_wrapper.style.backgroundColor = "#ddd";
}

function revertGreyOutElements() {
    joystick.style.backgroundColor = "rgb(14, 83, 187)";
    joystick_wrapper.style.backgroundColor = "rgb(64, 185, 255)";
}

ros.on('connection', function() {
    console.log('Connected to websocket server.');
    startstopButton.style.backgroundColor = "#0F0";
    startstopButton.textContent = "Disconnect from system";
    revertGreyOutElements();
});

ros.on('error', function(error) {
    console.log('Error connecting to websocket server: ', error);
    window.alert('Error connecting to websocket server');
    startstopButton.getElementById("startstop").style.backgroundColor = "#F00";
    startstopButton.textContent = "Connect to system";
    greyOutElements();
});

ros.on('close', function() {
    console.log('Connection to websocket server closed.');
    document.getElementById("startstop").style.backgroundColor = "#fff";
    startstopButton.textContent = "Connect to system";
    greyOutElements();
});

var ros = new ROSLIB.Ros();
var max_linear = 1.0;
var max_angular = 0.7;

const SHUTDOWN_ENUM = 0;
const REBOOT_ENUM = 1;

// Client for triggering system commands
var triggerSystemCommandClient = new ROSLIB.Service({
    ros : ros,
    name : '/trigger_system_command',
    serviceType : 'traethlin_interfaces/TriggerSystemCommand'
});

var shutdown_request = new ROSLIB.ServiceRequest({
    command : SHUTDOWN_ENUM
});

//var reboot_request = new ROSLIB.ServiceRequest({
//    command : REBOOT_ENUM
//});

function shutdown() {
    if (confirm("Confirm system shutdown") == true)
    {
      console.log("Shuting down.");
      triggerSystemCommandClient.callService(shutdown_request, function(result) {
            console.log('Result for service call on ' + triggerSystemCommandClient.name + ': ' + result.successful);
        });
    }
    else
    {
      console.log("Shut down cancelled.");
    }
}

//function reboot() {
//    if (confirm("Confirm system reboot") == true) {
//        triggerSystemCommandClient.callService(reboot_request, function(result) {
//            console.log('Result for service call on ' + triggerSystemCommandClient.name + ': ' + result.successful);
//        });
//    }
//}

// Client for gear selection.
var shiftGearCommandClient = new ROSLIB.Service({
    ros : ros,
    name : '/shift_gears',
    serviceType : 'traethlin_interfaces/ShiftGears'
});

var shift_low_gear_request = new ROSLIB.ServiceRequest({
    transmission_gear : 0
});

var shift_high_gear_request = new ROSLIB.ServiceRequest({
    transmission_gear : 1
});

/* level = 0: low; level = 1: high */
function shiftGear(level)
{
  if (level == 0)
  {
    console.log("Low gear");
    shiftGearCommandClient.callService(shift_low_gear_request, function(result) {
            console.log('Result for service call on ' + shiftGearCommandClient.name + ': ' + result.successful);
        });
  }
  else if (level == 1)
  {
    console.log("High gear");
    shiftGearCommandClient.callService(shift_high_gear_request, function(result) {
            console.log('Result for service call on ' + shiftGearCommandClient.name + ': ' + result.successful);
        });
  }
  else
  {
    log.console("shiftGear: wrong value (" + level
                + ", must be 0 (low) or 1 (high))");
  }
}

// Client for differential locking.
var diffLockCommandClient = new ROSLIB.Service({
    ros : ros,
    name : '/lock_differentials',
    serviceType : 'traethlin_interfaces/LockDifferentials'
});

var lock_diff_request = new ROSLIB.ServiceRequest({
    rear_differential_locked : false,
    front_differential_locked : false
});

/* true = lock; false: unlock */
function lockFrontDifferential(lock)
{
  lock_diff_request.front_differential_locked = lock;
  if (!lock)
  {
    console.log("Unlock front differential");
  }
  else  /* lock  is true */
  {
    console.log("Lock front differential");
  }

  diffLockCommandClient.callService(lock_diff_request, function(result) {
            console.log('Result for service call on ' + diffLockCommandClient.name
                        + ': ' + result.successful);
        });
}

/* true = lock; false: unlock */
function lockRearDifferential(lock)
{
  lock_diff_request.rear_differential_locked = lock;
  if (!lock)
  {
    console.log("Unlock rear differential");
  }
  else  /* lock  is true */
  {
    console.log("Lock rear differential");
  }

  diffLockCommandClient.callService(lock_diff_request, function(result) {
            console.log('Result for service call on ' + diffLockCommandClient.name
                        + ': ' + result.successful);
        });
}


// Topic for publishing twist messages
var twistTopic = new ROSLIB.Topic({
    ros : ros,
    name : '/cmd_vel',
    messageType : 'geometry_msgs/Twist'
})

function move(linear, angular) {
    var twist = new ROSLIB.Message({
        linear: {
            x : linear,
            y : 0,
            z : 0,
        },
        angular: {
            x : 0,
            y : 0,
            z : angular
        }
    });
    twistTopic.publish(twist);
}
