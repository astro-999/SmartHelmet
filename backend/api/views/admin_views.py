from rest_framework import permissions, status
from rest_framework.response import Response
from rest_framework.views import APIView
from django.db.models import Q
from django.utils import timezone
from datetime import timedelta

from ..models import RiderProfile, HelmetDevice, SensorReading, GPSLocation, Alert
from ..serializers import (
    AdminRiderListSerializer, RiderProfileSerializer,
    HelmetDeviceSerializer, AlertSerializer,
    SensorReadingSerializer, GPSLocationSerializer,
)


class AdminStatsView(APIView):
    """System-wide statistics for the admin dashboard."""
    permission_classes = [permissions.IsAdminUser]

    def get(self, request):
        now = timezone.now()
        today_start = now.replace(hour=0, minute=0, second=0, microsecond=0)

        total_riders = RiderProfile.objects.count()
        active_riders = RiderProfile.objects.filter(is_active_rider=True).count()
        online_devices = HelmetDevice.objects.filter(status='online').count()
        total_devices = HelmetDevice.objects.count()
        alerts_today = Alert.objects.filter(created_at__gte=today_start).count()
        critical_alerts = Alert.objects.filter(status='active', severity='critical').count()
        active_alerts = Alert.objects.filter(status='active').count()
        total_alerts = Alert.objects.count()

        # Recent alerts across all riders
        recent_alerts = Alert.objects.select_related('device', 'device__user')[:15]

        return Response({
            'total_riders': total_riders,
            'active_riders': active_riders,
            'online_devices': online_devices,
            'total_devices': total_devices,
            'alerts_today': alerts_today,
            'critical_alerts': critical_alerts,
            'active_alerts': active_alerts,
            'total_alerts': total_alerts,
            'recent_alerts': [{
                'id': a.id,
                'alert_type': a.alert_type,
                'type_display': a.get_alert_type_display(),
                'severity': a.severity,
                'status': a.status,
                'message': a.message,
                'rider_name': a.device.user.get_full_name() or a.device.user.username,
                'rider_id': getattr(getattr(a.device.user, 'rider_profile', None), 'rider_id', 'N/A'),
                'device_id': a.device.device_id,
                'latitude': a.latitude,
                'longitude': a.longitude,
                'created_at': a.created_at.isoformat(),
            } for a in recent_alerts],
        })


class AdminRiderListView(APIView):
    """List all riders with summary data."""
    permission_classes = [permissions.IsAdminUser]

    def get(self, request):
        profiles = RiderProfile.objects.select_related('user').all()

        # Optional search filter
        search = request.query_params.get('search', '').strip()
        if search:
            profiles = profiles.filter(
                Q(rider_id__icontains=search) |
                Q(user__username__icontains=search) |
                Q(user__first_name__icontains=search) |
                Q(user__last_name__icontains=search) |
                Q(user__email__icontains=search)
            )

        serializer = AdminRiderListSerializer(profiles, many=True)
        return Response(serializer.data)


class AdminRiderDetailView(APIView):
    """Detailed view of a specific rider for admin monitoring."""
    permission_classes = [permissions.IsAdminUser]

    def get(self, request, rider_id):
        try:
            profile = RiderProfile.objects.select_related('user').get(rider_id=rider_id)
        except RiderProfile.DoesNotExist:
            return Response({'error': 'Rider not found'}, status=status.HTTP_404_NOT_FOUND)

        user = profile.user
        device = HelmetDevice.objects.filter(user=user).first()

        # Rider profile data
        rider_data = {
            'rider_id': profile.rider_id,
            'username': user.username,
            'full_name': user.get_full_name() or user.username,
            'email': user.email,
            'phone': profile.phone,
            'address': profile.address,
            'blood_group': profile.blood_group,
            'license_number': profile.license_number,
            'is_active_rider': profile.is_active_rider,
            'date_joined': user.date_joined.isoformat(),
            'created_at': profile.created_at.isoformat(),
        }

        # Device data
        device_data = HelmetDeviceSerializer(device).data if device else None

        # Recent sensor readings (last 50)
        sensor_data = []
        if device:
            recent_readings = SensorReading.objects.filter(device=device)[:50]
            sensor_data = [{
                'timestamp': r.timestamp.isoformat(),
                'acc_x': r.acc_x, 'acc_y': r.acc_y, 'acc_z': r.acc_z,
                'gyro_x': r.gyro_x, 'gyro_y': r.gyro_y, 'gyro_z': r.gyro_z,
            } for r in reversed(list(recent_readings))]

        # GPS trail (last 100 points)
        gps_data = []
        latest_gps = None
        if device:
            gps_points = GPSLocation.objects.filter(device=device)[:100]
            gps_data = GPSLocationSerializer(reversed(list(gps_points)), many=True).data
            latest_gps_obj = GPSLocation.objects.filter(device=device).first()
            if latest_gps_obj:
                latest_gps = GPSLocationSerializer(latest_gps_obj).data

        # Alerts
        alerts = []
        alert_stats = {'total': 0, 'active': 0, 'resolved': 0}
        if device:
            all_alerts = Alert.objects.filter(device=device)
            alert_stats['total'] = all_alerts.count()
            alert_stats['active'] = all_alerts.filter(status='active').count()
            alert_stats['resolved'] = all_alerts.filter(status='resolved').count()
            alerts = AlertSerializer(all_alerts[:20], many=True).data

        return Response({
            'rider': rider_data,
            'device': device_data,
            'sensor_data': sensor_data,
            'gps_trail': gps_data,
            'latest_gps': latest_gps,
            'alerts': alerts,
            'alert_stats': alert_stats,
        })

    def patch(self, request, rider_id):
        """Update rider profile (admin action)."""
        try:
            profile = RiderProfile.objects.select_related('user').get(rider_id=rider_id)
        except RiderProfile.DoesNotExist:
            return Response({'error': 'Rider not found'}, status=status.HTTP_404_NOT_FOUND)

        # Updatable fields
        updatable = ['phone', 'address', 'blood_group', 'license_number', 'is_active_rider']
        for field in updatable:
            if field in request.data:
                setattr(profile, field, request.data[field])
        profile.save()

        return Response({
            'rider_id': profile.rider_id,
            'phone': profile.phone,
            'address': profile.address,
            'blood_group': profile.blood_group,
            'license_number': profile.license_number,
            'is_active_rider': profile.is_active_rider,
            'message': 'Rider profile updated successfully.',
        })
