from django.urls import path, include
from rest_framework.routers import DefaultRouter

from .views import (
    RegisterView, ProfileView,
    DeviceViewSet, SensorDataView, GPSDataView,
    AlertCreateView, AlertListView, AlertActionView,
    DashboardView, SensorHistoryView, GPSHistoryView,
    SOSTriggerView,
    AdminStatsView, AdminRiderListView, AdminRiderDetailView,
)
from .views.device import EmergencyContactViewSet

router = DefaultRouter()
router.register(r'contacts', EmergencyContactViewSet, basename='contact')
router.register(r'devices', DeviceViewSet, basename='device')

urlpatterns = [
    # Auth
    path('auth/register/', RegisterView.as_view(), name='register'),
    path('auth/profile/', ProfileView.as_view(), name='profile'),

    # CRUD endpoints (router)
    path('', include(router.urls)),

    # ESP32 device endpoints (no auth required)
    path('sensor-data/', SensorDataView.as_view(), name='sensor-data'),
    path('gps/', GPSDataView.as_view(), name='gps-data'),
    path('alert/', AlertCreateView.as_view(), name='alert-create'),

    # Dashboard & history
    path('dashboard/', DashboardView.as_view(), name='dashboard'),
    path('sensor-history/', SensorHistoryView.as_view(), name='sensor-history'),
    path('gps-history/', GPSHistoryView.as_view(), name='gps-history'),
    path('alerts/', AlertListView.as_view(), name='alert-list'),

    # Alert actions
    path('alerts/<int:pk>/<str:action_type>/', AlertActionView.as_view(), name='alert-action'),

    # SOS
    path('sos/', SOSTriggerView.as_view(), name='sos-trigger'),

    # Admin endpoints (staff only)
    path('admin/stats/', AdminStatsView.as_view(), name='admin-stats'),
    path('admin/riders/', AdminRiderListView.as_view(), name='admin-riders'),
    path('admin/riders/<str:rider_id>/', AdminRiderDetailView.as_view(), name='admin-rider-detail'),
]
