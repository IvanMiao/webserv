// =========================================
// File Upload Form Handler
// =========================================

document.addEventListener('DOMContentLoaded', function() {
    const uploadForm = document.getElementById('upload-form');
    const fileInput = document.getElementById('file-input');
    const fileInfo = document.getElementById('file-info');
    const uploadProgress = document.getElementById('upload-progress');
    const progressFill = document.getElementById('progress-fill');
    const progressText = document.getElementById('progress-text');
    const uploadResult = document.getElementById('upload-result');

    // Show file info when file is selected
    if (fileInput) {
        fileInput.addEventListener('change', function(e) {
            const file = e.target.files[0];
            if (file) {
                const fileSize = (file.size / 1024 / 1024).toFixed(2); // MB
                fileInfo.innerHTML = `
                    <strong>Selected:</strong> ${file.name}<br>
                    <strong>Size:</strong> ${fileSize} MB<br>
                    <strong>Type:</strong> ${file.type || 'Unknown'}
                `;
                fileInfo.classList.add('show');
            } else {
                fileInfo.classList.remove('show');
            }
        });
    }

    // Handle form submission
    if (uploadForm) {
        uploadForm.addEventListener('submit', function(e) {
            e.preventDefault();
            
            const file = fileInput.files[0];
            if (!file) {
                showResult('Please select a file', 'error');
                return;
            }

            // Check file size (10MB limit)
            const maxSize = 10 * 1024 * 1024; // 10MB
            if (file.size > maxSize) {
                showResult('File size exceeds 10MB limit', 'error');
                return;
            }

            // Prepare form data
            const formData = new FormData();
            formData.append('file', file);

            // Show progress bar
            uploadProgress.style.display = 'block';
            progressFill.style.width = '0%';
            progressText.textContent = '0%';

            // Create XMLHttpRequest for progress tracking
            const xhr = new XMLHttpRequest();

            // Track upload progress
            xhr.upload.addEventListener('progress', function(e) {
                if (e.lengthComputable) {
                    const percentComplete = (e.loaded / e.total) * 100;
                    progressFill.style.width = percentComplete + '%';
                    progressText.textContent = Math.round(percentComplete) + '%';
                }
            });

            // Handle completion
            xhr.addEventListener('load', function() {
                if (xhr.status === 200 || xhr.status === 201) {
                    showResult('File uploaded successfully!', 'success');
                    uploadForm.reset();
                    fileInfo.classList.remove('show');
                    
                    // Parse response if JSON
                    try {
                        const response = JSON.parse(xhr.responseText);
                        if (response.filename) {
                            showResult(`File uploaded: ${response.filename}`, 'success');
                        }
                    } catch (e) {
                        // Response is not JSON, use default message
                    }
                } else {
                    showResult(`Upload failed: ${xhr.statusText}`, 'error');
                }
                
                // Hide progress bar after 2 seconds
                setTimeout(() => {
                    uploadProgress.style.display = 'none';
                }, 2000);
            });

            // Handle errors
            xhr.addEventListener('error', function() {
                showResult('Upload failed: Network error', 'error');
                uploadProgress.style.display = 'none';
            });

            // Send request
            xhr.open('POST', '/upload');
            xhr.send(formData);
        });
    }

    // Show result message
    function showResult(message, type) {
        uploadResult.textContent = message;
        uploadResult.className = 'result-message ' + type;
        uploadResult.style.display = 'block';
        
        // Hide after 5 seconds
        setTimeout(() => {
            uploadResult.style.display = 'none';
        }, 5000);
    }
});
