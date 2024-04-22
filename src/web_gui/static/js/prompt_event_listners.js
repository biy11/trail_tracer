
/**
 * 0.1 Initial creation
 * 0.2 Refactored name-saving prompt functions, added const DOM elemets for tider code. 
 * 0.3 bugs fixes
 * 
 */

// ConStants for DOM elements 
const plotNamePrompt = document.getElementById('plot-name-prompt');
const noPlotNameErrorBtn = document.getElementById("no-entry-warning");
const saveTrailNameBtn = document.getElementById('saveTrailName');
const savePlotNameBtn = document.getElementById('savePlotName');
const trailNamePrompt = document.getElementById('trail-name-prompt');
const noEntryError = document.getElementById('no-entry-error');
const fileExistError = document.getElementById('file-exists-error');
const exitNamePromptBtn = document.getElementById('exit-name-prompt');
const entryError = document.getElementById('entry-error');
const existingFileError = document.getElementById('file-exist');
const exitPlotNamePromptBtn = document.getElementById('exit-plot-name-prompt');
const exitPromptBtn = document.getElementById('exit-prompt');
const trailPrompt = document.getElementById('trail-prompt');

document.addEventListener('DOMContentLoaded', () => { 
    // Function to initialize event listeners for the custom name prompt
    function initializeTrailNamePrompt() {
        saveTrailNameBtn.addEventListener('click', async function() { // SaveTrail function can be found in gui_script.js
            const trailNameInput = document.getElementById('trailNameInput');
            console.log(currentFiles);
            const trailName = trailNameInput.value.trim();

            noEntryError.style.display = 'none';
            fileExistError.style.display = 'none';

            if(trailName == ''){
                noEntryError.style.display = 'block';
                return;
            } 
            // Checks if file already exist with the same name and if so, file exist error shows.
            const fileExists = await checkTrailExists(trailName);
            if(fileExists){
                fileExistError.style.display = 'block';
                return;
            }
            checkSound(); //Play click sound as buton is clicked   
            controlRobot('manual-start'); // Sends message to back-end to signal start recording, function can be found in gui_script.js
            trail_name_emitter(trailName);
            isRecordingActive = true; // Update recording state
    
            // Update UI components
            manualStartBtn.textContent = 'End'; // Change the button to 'End'
            manualMoveBtn.style.display = 'none'; // Hide the 'manual-move' button during recording
    
            // Hide the trail name prompt and clear the input for next use
            trailNamePrompt.style.display = 'none';
            trailNameInput.value = '';

            fileExistError.style.display = 'none';
            noEntryError.style.display = 'none';
        });
        // When exit button is clicked, the prompt disapares and disabled buttons become live again.
        exitNamePromptBtn.addEventListener('click', function() {
            clickSound(); //Play click sound as buton is clicked
            trailNamePrompt.style.display = 'none';
            noEntryError.style.display = 'none';
            fileExistError.style.display = 'none';
            toggleModeBtn.disabled = false;
            manualMoveBtn.disabled = false;
            trailNameInput.value = '';
        });
    }

    // Asynchronous function to handel saving the trail name for the plotted trail
    async function handlePlotNameSaving() {
        savePlotNameBtn.addEventListener('click', async function() {
            checkSound(); // Play check sound as buton is clicked.

            // Trim and retrive plot name input from the 'plotNameInput' text filed.
            const plotNameInput = document.getElementById('plotNameInput');
            const plotName = plotNameInput.value.trim();

            // Initially hide error messages
            entryError.style.display = 'none';
            existingFileError.style.display = 'none';

            // If no input in entered, no input error shows on screen.
            if(plotName == ''){
                entryError.style.display = 'block';
                return;
            } 
            // Checks if file already exist with the same name and if so, file exist error shows.
            const fileExists = await checkTrailExists(plotName);
            if (fileExists){
                existingFileError.style.display = 'block';
                return;
            }
            // Log sucessful plotting to console and GUI logger
            console.log('Trail plotted with name:', plotName, coordinatesList);
            loggerLog("FILE PLOTTED: " + plotName + " \n   WITH POINTS: " + coordinatesList);
            
            saveTrail(plotName, coordinatesList); // Save trail name and coordinates

            // Hide prompt and reset input text field.
            plotNamePrompt.style.display = 'none';
            plotNameInput.value = '';
            resetPlottingUI(); // Reset UI and related states
            // Reset the recording active flag and coordinates list
            trailPlotting = false; // Flag to inidacte no acvtive plotting.
            coordinatesList = []; // Clear list of coordinates collected.
            });
        
        // Event listner for 'Exit' button click.
        exitPlotNamePromptBtn.addEventListener('click', function() {
            // Hide the plot-name prompt without saving if the user clicks 'Exit'
            entryError.style.display = 'none';
            existingFileError.style.display = 'none';
            plotNamePrompt.style.display = 'none';
            plotNameInput.value = ''; // Clear input filed.
            // Reset the plotting UI if plotting is active.
            if (trailPlotting) {
                resetPlottingUI();
            }
        }); 
    }

    exitPromptBtn.addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked
        trailPrompt.style.display = 'none';
        toggleModeBtn.disabled = false;
        plotTrailBtn.disabled = false;
        trailLoaded = false;
    });

    function resetPlottingUI() {
        // Reset the "Plot Trail" button text
        plotTrailBtn.textContent = 'Plot Trail';
        // Reset the "Load Trail" button text back from "Cancel" to its original text
        loadTrailBtn.textContent = 'Load Trail';
        // Re-enable buttons that were disabled during plotting
        toggleModeBtn.disabled = false;
        pauseBtn.disabled = false;
        emergencyStopBtn.disabled = false;
        // Remove any plotted waypoints from the map and clear the waypoints array
        wayPointMarkers.forEach(marker => marker.remove());
        wayPointMarkers = [];
        coordinatesList = []; // Clear the coordinates list
        map.off('click', mapClickHandler); // Disable the map click event handler for plotting new waypoints. Function mapClickHandler can be found in map_ui.js
        trailPlotting = false; // Flag for trail plotting is no longer being active
    }
    // Function for when page loads to set up the plot-name saving event listeners
    handlePlotNameSaving();
    initializeTrailNamePrompt();
    
});