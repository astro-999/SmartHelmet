"""
Seed initial data for SmartHelmetX development and testing.
Usage: python manage.py seed_data
"""

from django.core.management.base import BaseCommand
from django.contrib.auth.models import User
from api.models import RiderProfile, HelmetDevice, EmergencyContact


SAMPLE_RIDERS = [
    {
        'username': 'rider1', 'password': 'rider123',
        'first_name': 'Aarav', 'last_name': 'Sharma',
        'email': 'aarav@test.com',
        'phone': '+9779841000001', 'blood_group': 'A+',
        'license_number': 'BA-12345', 'address': 'Kathmandu, Nepal',
    },
    {
        'username': 'rider2', 'password': 'rider123',
        'first_name': 'Priya', 'last_name': 'Thapa',
        'email': 'priya@test.com',
        'phone': '+9779841000002', 'blood_group': 'B+',
        'license_number': 'BA-67890', 'address': 'Lalitpur, Nepal',
    },
    {
        'username': 'rider3', 'password': 'rider123',
        'first_name': 'Bikash', 'last_name': 'Gurung',
        'email': 'bikash@test.com',
        'phone': '+9779841000003', 'blood_group': 'O+',
        'license_number': 'BA-11223', 'address': 'Bhaktapur, Nepal',
    },
    {
        'username': 'rider4', 'password': 'rider123',
        'first_name': 'Sita', 'last_name': 'Rai',
        'email': 'sita@test.com',
        'phone': '+9779841000004', 'blood_group': 'AB+',
        'license_number': 'BA-44556', 'address': 'Pokhara, Nepal',
    },
    {
        'username': 'rider5', 'password': 'rider123',
        'first_name': 'Rajan', 'last_name': 'Maharjan',
        'email': 'rajan@test.com',
        'phone': '+9779841000005', 'blood_group': 'A-',
        'license_number': 'BA-78901', 'address': 'Kirtipur, Nepal',
    },
]


class Command(BaseCommand):
    help = 'Seed initial test data for SmartHelmetX development'

    def add_arguments(self, parser):
        parser.add_argument(
            '--reset', action='store_true',
            help='Delete existing test data before seeding',
        )

    def handle(self, *args, **options):
        if options['reset']:
            self.stdout.write(self.style.WARNING('Resetting test data...'))
            User.objects.filter(username__in=['admin'] + [r['username'] for r in SAMPLE_RIDERS]).delete()

        # --- Create admin user ---
        admin, created = User.objects.get_or_create(
            username='admin',
            defaults={
                'email': 'admin@test.com',
                'first_name': 'Admin',
                'last_name': 'User',
                'is_staff': True,
                'is_superuser': True,
            }
        )
        if created:
            admin.set_password('admin123')
            admin.save()
            self.stdout.write(self.style.SUCCESS("Created admin user 'admin' (password: admin123)"))
        else:
            # Ensure admin has staff privileges
            if not admin.is_staff:
                admin.is_staff = True
                admin.is_superuser = True
                admin.save()
                self.stdout.write(self.style.WARNING("Updated 'admin' to have staff privileges"))
            self.stdout.write("User 'admin' already exists.")

        # --- Ensure admin has a rider profile ---
        if not hasattr(admin, 'rider_profile'):
            RiderProfile.objects.create(
                user=admin,
                rider_id=RiderProfile.generate_rider_id(),
                phone='+9779800000000',
            )
            self.stdout.write(self.style.SUCCESS(f"Created rider profile for admin"))

        # --- Ensure admin has a device ---
        device, created = HelmetDevice.objects.get_or_create(
            device_id='SHX-0001',
            defaults={
                'user': admin,
                'name': 'SmartHelmetX',
                'status': 'online',
            }
        )
        if created:
            self.stdout.write(self.style.SUCCESS("Created device SHX-0001"))
        else:
            device.status = 'online'
            device.save(update_fields=['status', 'last_seen'])
            self.stdout.write("Device SHX-0001 already exists — set to online.")

        # --- Ensure admin has emergency contact ---
        contact, created = EmergencyContact.objects.get_or_create(
            user=admin,
            is_primary=True,
            defaults={
                'name': 'Emergency Contact',
                'phone': '+97798XXXXXXXX',
                'relationship': 'Family',
            }
        )
        if created:
            self.stdout.write(self.style.SUCCESS("Created emergency contact for admin"))
        else:
            self.stdout.write("Emergency contact already exists.")

        # --- Create sample riders ---
        self.stdout.write(self.style.MIGRATE_HEADING("\nCreating sample riders..."))
        device_statuses = ['online', 'offline', 'online', 'online', 'offline']
        battery_levels = [85, 42, 100, 67, 23]

        for i, rider_data in enumerate(SAMPLE_RIDERS):
            user, created = User.objects.get_or_create(
                username=rider_data['username'],
                defaults={
                    'email': rider_data['email'],
                    'first_name': rider_data['first_name'],
                    'last_name': rider_data['last_name'],
                }
            )
            if created:
                user.set_password(rider_data['password'])
                user.save()
                self.stdout.write(self.style.SUCCESS(
                    f"  Created rider '{user.username}' — {user.get_full_name()}"
                ))
            else:
                self.stdout.write(f"  Rider '{user.username}' already exists.")

            # Rider profile
            if not hasattr(user, 'rider_profile'):
                RiderProfile.objects.create(
                    user=user,
                    rider_id=RiderProfile.generate_rider_id(),
                    phone=rider_data.get('phone', ''),
                    blood_group=rider_data.get('blood_group', ''),
                    license_number=rider_data.get('license_number', ''),
                    address=rider_data.get('address', ''),
                )
                # Refresh to get the profile
                user.refresh_from_db()
                self.stdout.write(self.style.SUCCESS(
                    f"    Rider ID: {user.rider_profile.rider_id}"
                ))

            # Device
            device_id = f'SHX-{user.id:04d}'
            device, dev_created = HelmetDevice.objects.get_or_create(
                device_id=device_id,
                defaults={
                    'user': user,
                    'name': 'SmartHelmetX',
                    'status': device_statuses[i % len(device_statuses)],
                    'battery_level': battery_levels[i % len(battery_levels)],
                }
            )
            if dev_created:
                self.stdout.write(self.style.SUCCESS(f"    Device: {device_id}"))
            else:
                device.status = device_statuses[i % len(device_statuses)]
                device.battery_level = battery_levels[i % len(battery_levels)]
                device.save(update_fields=['status', 'battery_level', 'last_seen'])

            # Emergency contact
            EmergencyContact.objects.get_or_create(
                user=user, is_primary=True,
                defaults={
                    'name': f"{rider_data['first_name']}'s Family",
                    'phone': rider_data.get('phone', '+977-unknown'),
                    'relationship': 'Family',
                }
            )

        # --- Summary ---
        self.stdout.write(self.style.MIGRATE_HEADING("\n=== Summary ==="))
        for u in User.objects.all():
            rider_id = getattr(getattr(u, 'rider_profile', None), 'rider_id', 'N/A')
            devices = list(HelmetDevice.objects.filter(user=u).values_list('device_id', flat=True))
            role = 'ADMIN' if u.is_staff else 'Rider'
            self.stdout.write(
                f"  [{role}] {u.username} (Rider: {rider_id}) -> devices={devices}"
            )

        self.stdout.write(self.style.SUCCESS(
            "\nSeed complete! Login with: admin / admin123  |  rider1-5 / rider123"
        ))
