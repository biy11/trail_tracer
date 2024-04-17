// Contants for DOM elements 
const plotTrailBtn = document.getElementById('plot-trail-button');
const toggleModeBtn = document.getElementById('mode-toggle');
const pauseBtn = document.getElementById('pause-button');
const loadTrailBtn = document.getElementById('load-trail-button');
const emergencyStopBtn = document.getElementById('emergency-stop-button');
const plotNamePrompt = document.getElementById('plot-name-prompt');
const noPlotNameErrorBtn = document.getElementById("no-entry-warning");

document.addEventListener('DOMContentLoaded', () => {
    plotTrailBtn.addEventListener('click', function() {
        clickSound(); //Play click sound as buton is clicked
        if (!trailPlotting) {
            this.textContent = 'Save'; // Change the button text to "Save"

            wayPointMarkers.forEach(marker => marker.remove()); // Clearing markers from the map when plotting is stopped
            wayPointMarkers = []; // Reset the markers array
            
            // Disable other buttons except 'locate-me-button'
            toggleModeBtn.disabled = true;
            pauseBtn.disabled = true;
            loadTrailBtn.textContent = 'Cancel' 
            emergencyStopBtn.disabled = true;

            map.on('click', mapClickHandler); // Enable map clicking for coordinate collection, function mapClickHandler can be found in map_ui.js
    
            trailPlotting = true; // Set recording active flag
        } else {
            // Re-enable other buttons
            toggleModeBtn.disabled = false;
            pauseBtn.disabled = false;
            loadTrailBtn.disabled = false;
            emergencyStopBtn.disabled = false;
            plotNamePrompt.style.display = 'block';
            toggleModeBtn.disabled = true;
            //noPlotNameErrorBtn.style.display = 'none';

            map.off('click', mapClickHandler); // Disable map clicking for coordinate collection, function mapClickHandler can be found in map_ui.js
    
            console.log('Collected Coordinates:', coordinatesList); // Log the collected coordinates to the console
        }
    });  

    // Function to handle saving the trail name for the plotted trail
    async function handlePlotNameSaving() {
        document.getElementById('savePlotName').addEventListener('click', async function() {
            checkSound(); // Play check sound as buton is clicked.
            const plotNameInput = document.getElementById('plotNameInput');
            const plotName = plotNameInput.value.trim();

            document.getElementById('entry-error').style.display = 'none';
            document.getElementById('file-exist').style.display = 'none';

            if(plotName == ''){
                document.getElementById('entry-error').style.display = 'block';
                //document.getElementById('file-exist').style.display = 'none';
                return;
            } else if (currentFiles.includes(plotName + ".txt")){
                document.getElementById('file-exist').style.display = 'block';
                return;
            }
            console.log('Trail plotted with name:', plotName, coordinatesList);
            loggerLog("FILE PLOTTED: " + plotName + " \n   WITH POINTS: " + coordinatesList);
            //var waypoints = coordinatesList.map(coord => [coord.lat, coord.lng]);
            
            saveTrail(plotName, coordinatesList);

            plotNamePrompt.style.display = 'none';
            plotNameInput.value = '';
            resetPlottingUI();
            // Reset the recording active flag and coordinates list
            trailPlotting = false;
            coordinatesList = [];
            // const exits = await checkTrailExists(plotName);
            // if(exits == true){
            //     document.getElementById('file-exist').style.display = 'block';
            //     return;
            // } else{
            //     checkSound(); // Play check sound as buton is clicked.
            //     console.log('Trail plotted with name:', plotName);
            //     var waypoints = coordinatesList.map(coord => [coord.lat, coord.lng]);
                
            //     saveTrail(plotName, coordinatesList);
    
            //     plotNamePrompt.style.display = 'none';
            //     plotNameInput.value = '';
            //     resetPlottingUI();
            //     // Reset the recording active flag and coordinates list
            //     trailPlotting = false;
            //     coordinatesList = [];
            // }
            });

        document.getElementById('exit-plot-name-prompt').addEventListener('click', function() {
            // Hide the plot-name prompt without saving if the user clicks 'Exit'
            document.getElementById('entry-error').style.display = 'none';
            document.getElementById('file-exist').style.display = 'none';
            plotNamePrompt.style.display = 'none';
            plotNameInput.value = '';
            if (trailPlotting) {
                resetPlottingUI();
            }
        }); 
    }

    function checkTrailExists(plotName) {
        const dataToSend = { plotName: plotName };
    
        fetch('/check-trail-exists', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', },
            body: JSON.stringify(dataToSend),
        })
        .then(response => response.json())
        .then(data => {
            if (data.exists) {
                console.log("file exitst");
                return true;
                //document.getElementById('file-exists-error').style.display = 'block'; // Trail name exists, display the error message
                //plotNamePrompt.style.display = 'block'; // Keep the plot name prompt visible, allowing the user to change the name
                //document.getElementById('plotNameInput').focus(); // Refocus on the plotNameInput for the user to enter a new name

            } else {
                // Trail name does not exist, proceed to save and hide the error message if it was displayed
                //document.getElementById('file-exists-error').style.display = 'none';
                console.log("File saved")
                return false;
                //saveTrail(plotName, coordinatesList); // SaveTrail function can be found in gui_script.js
            }
        }).catch(error => console.error('Error:', error));
    }

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
    
});
