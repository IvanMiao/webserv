// Upload file to server
async function uploadFile() {
    const fileInput = document.getElementById('file-input');
    const status = document.getElementById('upload-status');
    const file = fileInput.files[0];
    
    if (!file) {
        status.textContent = 'Please select a file';
        return;
    }

    const formData = new FormData();
    formData.append('file', file);

    try {
        const res = await fetch('/uploads', {
            method: 'POST',
            body: formData
        });
        status.textContent = res.ok ? `✓ Uploaded ${file.name}` : `✗ Failed: ${res.status}`;
        if (res.ok) {
            fileInput.value = '';
            loadFiles();
        }
    } catch (e) {
        status.textContent = '✗ Error: ' + e.message;
    }
}

// Load file list from server
async function loadFiles() {
    const list = document.getElementById('file-list');
    try {
        const res = await fetch('/uploads');
        const html = await res.text();
        // Check if /uploads allows autoindex
		if (res.status === 403) {
			list.innerHTML = '<li>Error: autoindex is off</li>';
			return;
		}

		// Parse HTML response to get file links
        const parser = new DOMParser();
        const doc = parser.parseFromString(html, 'text/html');
        const links = doc.querySelectorAll('a');
        
        list.innerHTML = '';
        links.forEach(link => {
            const filename = link.textContent;
            // Filter out parent directory link
            if (filename && filename !== '../') {
                const li = document.createElement('li');
                li.innerHTML = `<span>${filename}</span>
                    <button class="delete-btn" onclick="deleteFile('${filename}')">Delete</button>`;
                list.appendChild(li);
            }
        });
    } catch (e) {
        list.innerHTML = '<li>Error loading files</li>';
    }
}

// Delete specified file
async function deleteFile(filename) {
    try {
        const res = await fetch(`/uploads/${filename}`, {method: 'DELETE'});
        if (res.ok) loadFiles();
        else alert('Delete failed: ' + res.status);
    } catch (e) {
        alert('Error: ' + e.message);
    }
}

// Test server connection and display request/response info
async function testServer() {
    const reqDisplay = document.getElementById('req-display');
    const resDisplay = document.getElementById('res-display');

    try {
        const response = await fetch('/echo', {method: 'GET'});
        const reqText = await response.text();
        reqDisplay.textContent = reqText;

        // Build response headers info
        let resHeaders = `HTTP/1.1 ${response.status} ${response.statusText}\n`;
        response.headers.forEach((val, key) => {
            resHeaders += `${key}: ${val}\n`;
        });
        resDisplay.textContent = resHeaders;
    } catch (e) {
        reqDisplay.textContent = "Error connecting to server: " + e;
        resDisplay.textContent = "Check console for details.";
    }
}

// Initialize on page load
testServer();
loadFiles();
