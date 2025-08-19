use adw::{glib, glib::clone, prelude::*, subclass::prelude::*};
use gtk::glib::{ffi::GType, translate::IntoGlib};

mod imp {
    use glib::ParamSpec;
    use gtk::glib::ParamSpecObject;
    use std::sync::LazyLock;

    use super::*;

    #[derive(Default)]
    pub struct BzScrolledWindow {}

    #[glib::object_subclass]
    impl ObjectSubclass for BzScrolledWindow {
        // `NAME` needs to match `class` attribute of template
        const NAME: &'static str = "BzScrolledWindow";
        type Type = super::BzScrolledWindow;
        type ParentType = adw::Bin;
    }

    impl ObjectImpl for BzScrolledWindow {
        fn constructed(&self) {
            self.parent_constructed();
            let obj = self.obj();

            let scrolled_window = gtk::ScrolledWindow::new();
            scrolled_window.set_hscrollbar_policy(gtk::PolicyType::Never);
            obj.set_child(Some(&scrolled_window));

            let spring_animation = adw::SpringAnimation::new(
                obj.as_ref(),
                0.0,
                0.0,
                adw::SpringParams::new(2.0, 1.0, 5000.0),
                adw::CallbackAnimationTarget::new(clone!(
                    #[strong]
                    scrolled_window,
                    move |v| {
                        scrolled_window.vadjustment().set_value(v);
                    }
                )),
            );
            spring_animation.set_epsilon(0.0005);

            let scroll_controller =
                gtk::EventControllerScroll::new(gtk::EventControllerScrollFlags::VERTICAL);

            scroll_controller.connect_scroll(clone!(
                #[strong]
                scrolled_window,
                #[strong]
                spring_animation,
                #[strong]
                scroll_controller,
                move |_, _, y| {
                    if scroll_controller.unit() == gtk::gdk::ScrollUnit::Wheel {
                        let min = scrolled_window.vadjustment().lower();
                        let max = scrolled_window.vadjustment().upper();
                        let value_to = (if spring_animation.state() == adw::AnimationState::Playing
                        {
                            spring_animation.value_to()
                        } else {
                            scrolled_window.vadjustment().value()
                        } + y * 250.0)
                            .clamp(min, max);

                        spring_animation.set_value_from(scrolled_window.vadjustment().value());
                        spring_animation.set_value_to(value_to);
                        spring_animation.set_initial_velocity(spring_animation.velocity());
                        spring_animation.play();
                        glib::signal::Propagation::Stop
                    } else {
                        glib::signal::Propagation::Proceed
                    }
                }
            ));
            scroll_controller.set_propagation_phase(gtk::PropagationPhase::Capture);
            obj.add_controller(scroll_controller);

            scrolled_window.connect_scroll_child(clone!(
                #[strong]
                spring_animation,
                move |_, _, _| {
                    spring_animation.reset();
                    return true;
                }
            ));
        }

        fn properties() -> &'static [ParamSpec] {
            static PARAM_SPECS: LazyLock<Vec<ParamSpec>> =
                LazyLock::new(|| vec![ParamSpecObject::builder::<gtk::Widget>("child").build()]);
            PARAM_SPECS.as_ref()
        }

        fn property(&self, _id: usize, pspec: &ParamSpec) -> glib::Value {
            let obj = self.obj();
            match pspec.name() {
                "child" => obj.child().to_value(),
                _ => unimplemented!(),
            }
        }

        fn set_property(&self, _id: usize, value: &glib::Value, pspec: &ParamSpec) {
            let obj = self.obj();
            match pspec.name() {
                "child" => {
                    if let Ok(v) = value.get::<gtk::Widget>() {
                        if let Some(scrolled_window) =
                            obj.child().and_downcast::<gtk::ScrolledWindow>()
                        {
                            scrolled_window.set_child(Some(&v));
                        }
                    }
                }
                _ => unimplemented!(),
            }
        }
    }

    impl WidgetImpl for BzScrolledWindow {}

    impl BinImpl for BzScrolledWindow {}
}

glib::wrapper! {
    pub struct BzScrolledWindow(ObjectSubclass<imp::BzScrolledWindow>)
    @extends adw::Bin, gtk::Widget,
    @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

#[no_mangle]
pub extern "C" fn bz_scrolled_window_get_type() -> GType {
    <BzScrolledWindow as StaticType>::static_type().into_glib()
}
