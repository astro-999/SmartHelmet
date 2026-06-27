from django.contrib import admin
from .models import RiderProfile, EmergencyContact, HelmetDevice, SensorReading, GPSLocation, Alert


@admin.register(RiderProfile)
class RiderProfileAdmin(admin.ModelAdmin):
    list_display = ('rider_id', 'user', 'phone', 'blood_group', 'is_active_rider', 'created_at')
    list_filter = ('is_active_rider', 'blood_group')
    search_fields = ('rider_id', 'user__username', 'user__first_name', 'user__last_name', 'phone')
    readonly_fields = ('rider_id', 'created_at')


@admin.register(EmergencyContact)
class EmergencyContactAdmin(admin.ModelAdmin):
    list_display = ('name', 'phone', 'user', 'relationship', 'is_primary')
    list_filter = ('is_primary',)
    search_fields = ('name', 'phone')


@admin.register(HelmetDevice)
class HelmetDeviceAdmin(admin.ModelAdmin):
    list_display = ('device_id', 'name', 'user', 'status', 'battery_level', 'last_seen')
    list_filter = ('status',)
    search_fields = ('device_id', 'name')
    readonly_fields = ('last_seen', 'created_at')


@admin.register(SensorReading)
class SensorReadingAdmin(admin.ModelAdmin):
    list_display = ('device', 'timestamp', 'acc_x', 'acc_y', 'acc_z')
    list_filter = ('device',)
    readonly_fields = ('timestamp',)
    date_hierarchy = 'timestamp'


@admin.register(GPSLocation)
class GPSLocationAdmin(admin.ModelAdmin):
    list_display = ('device', 'timestamp', 'latitude', 'longitude', 'speed')
    list_filter = ('device',)
    readonly_fields = ('timestamp',)
    date_hierarchy = 'timestamp'


@admin.register(Alert)
class AlertAdmin(admin.ModelAdmin):
    list_display = ('alert_type', 'severity', 'status', 'device', 'created_at')
    list_filter = ('alert_type', 'severity', 'status')
    search_fields = ('message',)
    readonly_fields = ('created_at',)
    date_hierarchy = 'created_at'
