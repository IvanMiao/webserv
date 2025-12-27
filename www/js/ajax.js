// =========================================
// AJAX Form Handler (for contact form)
// =========================================

document.addEventListener('DOMContentLoaded', function() {
    const contactForm = document.getElementById('contact-form');
    const formResult = document.getElementById('form-result');

    if (contactForm) {
        contactForm.addEventListener('submit', function(e) {
            e.preventDefault();
            
            // Get form data
            const formData = new FormData(contactForm);
            
            // Convert to URL-encoded string
            const urlEncodedData = new URLSearchParams(formData).toString();
            
            // Show loading state
            const submitButton = contactForm.querySelector('button[type="submit"]');
            const originalButtonText = submitButton.textContent;
            submitButton.textContent = 'Sending...';
            submitButton.disabled = true;
            
            // Send AJAX request
            fetch(contactForm.action, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded'
                },
                body: urlEncodedData
            })
            .then(response => {
                if (response.ok) {
                    return response.text();
                } else {
                    throw new Error('Server returned ' + response.status);
                }
            })
            .then(data => {
                showFormResult('Message sent successfully!', 'success');
                contactForm.reset();
            })
            .catch(error => {
                showFormResult('Failed to send message: ' + error.message, 'error');
            })
            .finally(() => {
                // Restore button
                submitButton.textContent = originalButtonText;
                submitButton.disabled = false;
            });
        });
    }

    function showFormResult(message, type) {
        formResult.textContent = message;
        formResult.className = 'result-message ' + type;
        formResult.style.display = 'block';
        
        // Hide after 5 seconds
        setTimeout(() => {
            formResult.style.display = 'none';
        }, 5000);
    }
});
