from rest_framework import serializers
from django.contrib.auth.models import User
from .models import RiderProfile, EmergencyContact, HelmetDevice, SensorReading, GPSLocation, Alert


class UserRegistrationSerializer(serializers.ModelSerializer):
    password = serializers.CharField(write_only=True, min_length=6)
    password2 = serializers.CharField(write_only=True, min_length=6)

    class Meta:
        model = User
        fields = ('id', 'username', 'email', 'password', 'password2', 'first_name', 'last_name')

    def validate(self, data):
        if data['password'] != data['password2']:
            raise serializers.ValidationError({'password': 'Passwords do not match.'})
        return data

    def create(self, validated_data):
        validated_data.pop('password2')
        user = User.objects.create_user(**validated_data)
        # Create rider profile with unique ID
        RiderProfile.objects.create(
            user=user,
            rider_id=RiderProfile.generate_rider_id(),
        )
        # Create a default helmet device for the user
        HelmetDevice.objects.create(
            user=user,
            device_id=f'SHX-{user.id:04d}',
            name='SmartHelmetX'
        )
        return user


class UserSerializer(serializers.ModelSerializer):
    is_staff = serializers.BooleanField(read_only=True)
    rider_id = serializers.SerializerMethodField()

    class Meta:
        model = User
        fields = ('id', 'username', 'email', 'first_name', 'last_name', 'is_staff', 'rider_id')

    def get_rider_id(self, obj):
        profile = getattr(obj, 'rider_profile', None)
        return profile.rider_id if profile else None


class EmergencyContactSerializer(serializers.ModelSerializer):
    class Meta:
        model = EmergencyContact
        fields = ('id', 'name', 'phone', 'relationship', 'is_primary', 'created_at')
        read_only_fields = ('id', 'created_at')


class HelmetDeviceSerializer(serializers.ModelSerializer):
    class Meta:
        model = HelmetDevice
        fields = ('id', 'device_id', 'name', 'ble_mac', 'status', 'battery_level',
                  'firmware_version', 'fall_threshold', 'no_move_timeout', 'last_seen', 'created_at')
        read_only_fields = ('id', 'device_id', 'last_seen', 'created_at')


class SensorReadingSerializer(serializers.ModelSerializer):
    class Meta:
        model = SensorReading
        fields = ('id', 'device', 'timestamp', 'acc_x', 'acc_y', 'acc_z',
                  'gyro_x', 'gyro_y', 'gyro_z')
        read_only_fields = ('id', 'timestamp')


class SensorDataInputSerializer(serializers.Serializer):
    """For ESP32 posting sensor data."""
    device_id = serializers.CharField()
    acc_x = serializers.FloatField()
    acc_y = serializers.FloatField()
    acc_z = serializers.FloatField()
    gyro_x = serializers.FloatField(default=0)
    gyro_y = serializers.FloatField(default=0)
    gyro_z = serializers.FloatField(default=0)


class GPSLocationSerializer(serializers.ModelSerializer):
    class Meta:
        model = GPSLocation
        fields = ('id', 'device', 'timestamp', 'latitude', 'longitude',
                  'speed', 'altitude', 'accuracy')
        read_only_fields = ('id', 'timestamp')


class GPSInputSerializer(serializers.Serializer):
    """For ESP32 posting GPS data."""
    device_id = serializers.CharField()
    latitude = serializers.FloatField()
    longitude = serializers.FloatField()
    speed = serializers.FloatField(default=0)
    altitude = serializers.FloatField(default=0)
    accuracy = serializers.FloatField(default=0)


class AlertSerializer(serializers.ModelSerializer):
    device_name = serializers.CharField(source='device.name', read_only=True)
    type_display = serializers.CharField(source='get_alert_type_display', read_only=True)

    class Meta:
        model = Alert
        fields = ('id', 'device', 'device_name', 'alert_type', 'type_display', 'status',
                  'severity', 'latitude', 'longitude', 'message', 'acc_magnitude',
                  'sms_sent', 'resolved_at', 'created_at')
        read_only_fields = ('id', 'created_at')


class AlertInputSerializer(serializers.Serializer):
    """For ESP32 posting alerts."""
    device_id = serializers.CharField()
    alert_type = serializers.ChoiceField(choices=['fall', 'sos', 'no_movement', 'impact'])
    severity = serializers.ChoiceField(choices=['low', 'medium', 'high', 'critical'], default='high')
    latitude = serializers.FloatField(required=False)
    longitude = serializers.FloatField(required=False)
    message = serializers.CharField(default='')
    acc_magnitude = serializers.FloatField(default=0)


class DashboardSerializer(serializers.Serializer):
    """Dashboard summary data."""
    device = HelmetDeviceSerializer()
    latest_gps = GPSLocationSerializer(allow_null=True)
    total_alerts = serializers.IntegerField()
    active_alerts = serializers.IntegerField()
    alerts_today = serializers.IntegerField()
    latest_alerts = AlertSerializer(many=True)
    sensor_summary = serializers.DictField()


# ============ RIDER PROFILE ============

class RiderProfileSerializer(serializers.ModelSerializer):
    class Meta:
        model = RiderProfile
        fields = ('rider_id', 'phone', 'address', 'blood_group',
                  'license_number', 'is_active_rider', 'created_at')
        read_only_fields = ('rider_id', 'created_at')


# ============ ADMIN SERIALIZERS ============

class AdminRiderListSerializer(serializers.Serializer):
    """Rider summary for admin riders list."""
    id = serializers.IntegerField(source='user.id')
    rider_id = serializers.CharField()
    username = serializers.CharField(source='user.username')
    full_name = serializers.SerializerMethodField()
    email = serializers.EmailField(source='user.email')
    phone = serializers.CharField()
    blood_group = serializers.CharField()
    license_number = serializers.CharField()
    is_active_rider = serializers.BooleanField()
    created_at = serializers.DateTimeField()
    device = serializers.SerializerMethodField()
    latest_gps = serializers.SerializerMethodField()
    alert_count = serializers.SerializerMethodField()
    active_alerts = serializers.SerializerMethodField()

    def get_full_name(self, obj):
        name = obj.user.get_full_name()
        return name if name else obj.user.username

    def get_device(self, obj):
        device = HelmetDevice.objects.filter(user=obj.user).first()
        if device:
            return {
                'device_id': device.device_id,
                'name': device.name,
                'status': device.status,
                'battery_level': device.battery_level,
                'last_seen': device.last_seen.isoformat() if device.last_seen else None,
            }
        return None

    def get_latest_gps(self, obj):
        device = HelmetDevice.objects.filter(user=obj.user).first()
        if device:
            gps = GPSLocation.objects.filter(device=device).first()
            if gps:
                return {
                    'latitude': gps.latitude,
                    'longitude': gps.longitude,
                    'speed': gps.speed,
                    'timestamp': gps.timestamp.isoformat(),
                }
        return None

    def get_alert_count(self, obj):
        return Alert.objects.filter(device__user=obj.user).count()

    def get_active_alerts(self, obj):
        return Alert.objects.filter(device__user=obj.user, status='active').count()
