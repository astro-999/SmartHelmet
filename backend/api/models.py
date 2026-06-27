from django.db import models
from django.contrib.auth.models import User


class RiderProfile(models.Model):
    """Extended rider profile with unique rider ID."""
    user = models.OneToOneField(User, on_delete=models.CASCADE, related_name='rider_profile')
    rider_id = models.CharField(max_length=20, unique=True, db_index=True)  # e.g., RDR-0001
    phone = models.CharField(max_length=20, blank=True, default='')
    address = models.TextField(blank=True, default='')
    blood_group = models.CharField(max_length=5, blank=True, default='')
    license_number = models.CharField(max_length=50, blank=True, default='')
    is_active_rider = models.BooleanField(default=True)
    created_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ['rider_id']

    def __str__(self):
        return f"{self.rider_id} — {self.user.get_full_name() or self.user.username}"

    @staticmethod
    def generate_rider_id():
        """Generate the next sequential rider ID."""
        last = RiderProfile.objects.order_by('-id').first()
        if last:
            try:
                num = int(last.rider_id.split('-')[1]) + 1
            except (IndexError, ValueError):
                num = RiderProfile.objects.count() + 1
        else:
            num = 1
        return f"RDR-{num:04d}"


class EmergencyContact(models.Model):
    """Emergency contact for the user."""
    user = models.ForeignKey(User, on_delete=models.CASCADE, related_name='emergency_contacts')
    name = models.CharField(max_length=100)
    phone = models.CharField(max_length=20)
    relationship = models.CharField(max_length=50, blank=True, default='')
    is_primary = models.BooleanField(default=False)
    created_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ['-is_primary', 'name']

    def __str__(self):
        return f"{self.name} ({self.phone})"


class HelmetDevice(models.Model):
    """Represents a smart helmet device."""
    STATUS_CHOICES = [
        ('online', 'Online'),
        ('offline', 'Offline'),
        ('pairing', 'Pairing'),
    ]

    user = models.ForeignKey(User, on_delete=models.CASCADE, related_name='devices')
    device_id = models.CharField(max_length=50, unique=True)
    name = models.CharField(max_length=100, default='SmartHelmetX')
    ble_mac = models.CharField(max_length=17, blank=True, default='')
    status = models.CharField(max_length=10, choices=STATUS_CHOICES, default='offline')
    battery_level = models.IntegerField(default=100)
    firmware_version = models.CharField(max_length=20, default='1.0.0')
    fall_threshold = models.FloatField(default=1.8, help_text='G-force threshold for fall detection')
    no_move_timeout = models.IntegerField(default=8000, help_text='No movement timeout in ms')
    last_seen = models.DateTimeField(auto_now=True)
    created_at = models.DateTimeField(auto_now_add=True)

    def __str__(self):
        return f"{self.name} ({self.device_id})"


class SensorReading(models.Model):
    """Timestamped accelerometer/gyroscope reading from the helmet."""
    device = models.ForeignKey(HelmetDevice, on_delete=models.CASCADE, related_name='readings')
    timestamp = models.DateTimeField(auto_now_add=True, db_index=True)
    # Accelerometer (g-force)
    acc_x = models.FloatField(default=0)
    acc_y = models.FloatField(default=0)
    acc_z = models.FloatField(default=1.0)
    # Gyroscope (degrees/sec)
    gyro_x = models.FloatField(default=0)
    gyro_y = models.FloatField(default=0)
    gyro_z = models.FloatField(default=0)

    class Meta:
        ordering = ['-timestamp']
        indexes = [
            models.Index(fields=['device', '-timestamp']),
        ]

    def __str__(self):
        return f"Reading {self.device.device_id} @ {self.timestamp}"


class GPSLocation(models.Model):
    """Timestamped GPS location from the helmet."""
    device = models.ForeignKey(HelmetDevice, on_delete=models.CASCADE, related_name='locations')
    timestamp = models.DateTimeField(auto_now_add=True, db_index=True)
    latitude = models.FloatField()
    longitude = models.FloatField()
    speed = models.FloatField(default=0, help_text='Speed in km/h')
    altitude = models.FloatField(default=0)
    accuracy = models.FloatField(default=0, help_text='GPS accuracy in meters')

    class Meta:
        ordering = ['-timestamp']
        indexes = [
            models.Index(fields=['device', '-timestamp']),
        ]

    def __str__(self):
        return f"GPS {self.latitude},{self.longitude} @ {self.timestamp}"


class Alert(models.Model):
    """Fall detection or SOS alert."""
    TYPE_CHOICES = [
        ('fall', 'Fall Detected'),
        ('sos', 'SOS Triggered'),
        ('no_movement', 'No Movement'),
        ('impact', 'High Impact'),
    ]
    STATUS_CHOICES = [
        ('active', 'Active'),
        ('cancelled', 'Cancelled'),
        ('resolved', 'Resolved'),
    ]
    SEVERITY_CHOICES = [
        ('low', 'Low'),
        ('medium', 'Medium'),
        ('high', 'High'),
        ('critical', 'Critical'),
    ]

    device = models.ForeignKey(HelmetDevice, on_delete=models.CASCADE, related_name='alerts')
    alert_type = models.CharField(max_length=15, choices=TYPE_CHOICES)
    status = models.CharField(max_length=10, choices=STATUS_CHOICES, default='active')
    severity = models.CharField(max_length=10, choices=SEVERITY_CHOICES, default='high')
    latitude = models.FloatField(null=True, blank=True)
    longitude = models.FloatField(null=True, blank=True)
    message = models.TextField(default='')
    acc_magnitude = models.FloatField(default=0, help_text='Acceleration magnitude at time of alert')
    sms_sent = models.BooleanField(default=False)
    resolved_at = models.DateTimeField(null=True, blank=True)
    created_at = models.DateTimeField(auto_now_add=True, db_index=True)

    class Meta:
        ordering = ['-created_at']

    def __str__(self):
        return f"{self.get_alert_type_display()} - {self.status} @ {self.created_at}"
