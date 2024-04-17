/**
 * This is a JavaScript for handeling interactions, dasboard updates and robot controls.
 * This includes initilising the map, robot and user location markers, routing direction 
 * handeling, and updating telemetray data on the dashboard.
 */

// Map and UI Interactions

/**
 * 0.1 initial creation
 * 0.2 set-up for ros2
 * 0.3 comunication with ros2
 * 0.4 bugs fixes
 * 0.5 manual display creatoion
 * 0.6 adaptation for manual display
 * 0.7 file storing for manual display
 * 0.8 Added prompt for loading file
 * 0.9 Fixed bugs for loading files.
 * 1.0 Added button to manually move robot via joystick.
 * 1.1 Added prompt for plotted trail.
 * 1.2 Refactored
 * 1.3 renamed to event_listeners.js
 * 1.4 Added comments
 * 1.5 Added sound effects to buttons and toggle.
 * 1.6 Added event listner for "clear-log" button.
 * 
 */

var map; // Reference to Leaflet map instance.
var robotMarker; // Marker for the robot's location.
var userLocationMarker; // Marker for the user's location.
var currentRoutingControl = null; // To hold the current routing control object.
var directionsSet = false; // Flag to track if directions are currently set.
var isRecordingActive = false; // Variable to track whether a trail recording is currently active.
var runingTrail = false; //Tracks if the trail is currently runing.
var trailLoaded = false; //Tracks if the trail has been loaded.
var trailPlotting = false; //Flag to track ewather a trail is being plotted or not.
var coordinatesList = []; // To hold the coordinates clicked on the map
var wayPointMarkers = []; // Array to hold markers for plotted points.
var manualMove = false; // Flag to tell if the robot is being moved without a recording/


// Map initialization and event listeners on DOM content load.
document.addEventListener('DOMContentLoaded', () => {

    window.addEventListener('beforeunload', function(e){
        if (isRecordingActive){
            var message = 'Current trail recording will end if you leave the page. Are you sure you wish to leave?'
            e.returnValue = message;
            return message;
        }
    });

    // Dynamic functionality for "locate-me-button"
    document.getElementById('locate-me-button').addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked
        //Cancel direction if direction is set, if not, locate robot.
        if (directionsSet) {
            cancelDirections(); // Function can be found in map_ui.js
        } else {
            locateRobot(); // Function can be found in map_ui.js
        }
    });

    // Mode-toggle event listeners
    toggleModeBtn.addEventListener('click', function(){
        toggleSound(); // Play toggle sound as buton is clicked 
        if (!isRecordingActive){
            toggleMode() // Function can be found in gui_script.js
        }else{
            return;
        }
    });

    
    document.getElementById('manual-start').addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked

        // Check if recording is not active to start a new recording
        if (!isRecordingActive) {
            document.getElementById('trail-name-prompt').style.display = 'block';
            document.getElementById('no-entry-warning').style.display = 'none';

            toggleModeBtn.disabled = true;
            document.getElementById('manual-move').disabled = true;
        } else {
            // Code to execute when stopping the recording
            controlRobot('manual-stop'); // Signal the backend that the recording has stopped, function can be found in gui_script.js
            isRecordingActive = false; // Update the recording state
    
            // Update UI components
            this.textContent = 'Record Trail'; // Change the button text back to 'Record Trail'
            document.getElementById('manual-move').style.display = 'block'; // Re-appear the 'manual-move' button
            //toggleModeBtn.disabled = false;
            document.getElementById('manual-move').disabled = false;
        }
    });

    // Function to initialize event listeners for the custom name prompt
    function initializeTrailNamePrompt() {
        document.getElementById('saveTrailName').addEventListener('click', function() { // SaveTrail function can be found in gui_script.js
            const trailNameInput = document.getElementById('trailNameInput');
            console.log(currentFiles);
            const trailName = trailNameInput.value.trim();

            document.getElementById('no-entry-error').style.display = 'none';
            document.getElementById('file-exists-error').style.display = 'none';

            if(trailName == ''){
                document.getElementById('no-entry-error').style.display = 'block';
                //document.getElementById('file-exist').style.display = 'none';
                return;
            } else if (currentFiles.includes(trailName + ".txt")){
                document.getElementById('file-exists-error').style.display = 'block';
                return;
            }
            checkSound(); //Play click sound as buton is clicked   
            controlRobot('manual-start'); // Sends message to back-end to signal start recording, function can be found in gui_script.js
            trail_name_emitter(trailName);
            isRecordingActive = true; // Update recording state
    
            // Update UI components
            document.getElementById('manual-start').textContent = 'End'; // Change the button to 'End'
            document.getElementById('manual-move').style.display = 'none'; // Hide the 'manual-move' button during recording
    
            // Hide the trail name prompt and clear the input for next use
            document.getElementById('trail-name-prompt').style.display = 'none';
            trailNameInput.value = '';

            document.getElementById('file-exists-error').style.display = 'none';
            document.getElementById('no-entry-error').style.display = 'none';
        });
        // When exit button is clicked, the prompt disapares and disabled buttons become live again.
        document.getElementById('exit-name-prompt').addEventListener('click', function() {
            clickSound(); //Play click sound as buton is clicked
            document.getElementById('trail-name-prompt').style.display = 'none';
            document.getElementById('no-entry-error').style.display = 'none';
            document.getElementById('file-exists-error').style.display = 'none';
            toggleModeBtn.disabled = false;
            document.getElementById('manual-move').disabled = false;
            trailNameInput.value = '';
        });
    }
    
    // When exit button is clicked, the prompt disapares and disabled buttons become live again.
    document.getElementById('exit-plot-name-prompt').addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked
        document.getElementById('no-entry-error').style.display = 'none';
        toggleModeBtn.disabled = false;
        document.getElementById('manual-move').disabled = false;
    });
    initializeTrailNamePrompt();

    document.getElementById('manual-move').addEventListener('click', function(){
        clickSound(); //Play click sound as buton is clicked
        if(!manualMove){
            controlRobot('manual-move'); // Sends message to back-end, function can be found in gui_script.js
            this.textContent = "Stop";
            manualMove = true;

            // Re-enable other buttons
            toggleModeBtn.disabled = true;
            document.getElementById('manual-start').style.display = 'none';
        } else {
            controlRobot('manual-stop'); // Sends message to back-end, function can be found in gui_script.js
            this.textContent = "Manualy Move Robot";
            // Re-enable other buttons
            toggleModeBtn.disabled = false;
            document.getElementById('manual-start').style.display = 'block';
            manualMove = false;
        }
    });

    pauseBtn.addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked
        controlRobot('pause')

        });

    loadTrailBtn.addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked
        if (this.textContent === 'Cancel') {
            // If the button is in "Cancel" mode, revert the 'plot-trail-button' text and state
            plotTrailBtn.textContent = 'Plot Trail';
            
            // Revert any other changes made for plotting the trail
            wayPointMarkers.forEach(marker => marker.remove());
            wayPointMarkers = [];
            map.off('click', mapClickHandler); // Function mapClickHandler can be found in map_ui.js
            trailPlotting = false;
            coordinatesList = [];
            
            // Re-enable disabled buttons due to plotting
            toggleModeBtn.disabled = false;
            pauseBtn.disabled = false;
            emergencyStopBtn.disabled = false;
            
            // Revert the 'load-trail-button' text back to its original
            this.textContent = 'Load Trail';
        } else if(!trailLoaded){
            document.getElementById('trail-prompt').style.display = 'block';
            toggleModeBtn.disabled = true;
            plotTrailBtn.disabled = true;
            this.textContent = 'Load Trail';
            trailLoaded = true;
        } else{
            loadTrailBtn.textContent = 'Load Trail';
            document.getElementById('trail-prompt').style.display = 'none';
            toggleModeBtn.disabled = false;
            plotTrailBtn.disabled = false;
            plotTrailBtn.style.display = 'block';
            emergencyStopBtn.style.display = 'none'; 
            pauseBtn.style.display = 'none'; 

            trailLoaded = false;
            runingTrail = false;
        }
    });

    document.getElementById('exit-prompt').addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked
        document.getElementById('trail-prompt').style.display = 'none';
        toggleModeBtn.disabled = false;
        plotTrailBtn.disabled = false;
        trailLoaded = false;
    });

    emergencyStopBtn.addEventListener('click', function(){
    clickSound(); //Play click sound as buton is clicked
    controlRobot('emergency-stop')});
    
    document.getElementById("clear-log").addEventListener('click', ()=> clearLog());
});

